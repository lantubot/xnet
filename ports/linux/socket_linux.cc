// Copyright (c) 2026 XNet Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "socket_linux.h"

#include <arpa/inet.h>  // htons, inet_pton
#include <netdb.h>      // getaddrinfo, freeaddrinfo

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <new>  // std::nothrow

#include "xnet/socket.h"

namespace xnet {

// ============================================================================
// LinuxTcpSocket
// ============================================================================

LinuxTcpSocket::LinuxTcpSocket() : fd_(-1) {}

LinuxTcpSocket::~LinuxTcpSocket() { close(); }

// ──
// errno_to_status（错误码转状态）────────────────────────────────────────────
// 静态方法
Status LinuxTcpSocket::errno_to_status(int err) {
  switch (err) {
    case ECONNREFUSED:
      return Status::CONNECTION_REFUSED;
    case ECONNRESET:
      return Status::CONNECTION_RESET;
    case ETIMEDOUT:
      return Status::TIMEOUT;
    case EAGAIN:
#if EAGAIN != EWOULDBLOCK
    case EWOULDBLOCK:
#endif
      return Status::TIMEOUT;
    case EHOSTUNREACH:
    case ENETUNREACH:
      return Status::CONNECTION_REFUSED;
    case EAFNOSUPPORT:
    case EPROTONOSUPPORT:
    case EPROTOTYPE:
      return Status::UNSUPPORTED_PROTOCOL;
    case ENOMEM:
      return Status::OUT_OF_MEMORY;
    case EINTR:
      return Status::IO_ERROR;
    default:
      return Status::IO_ERROR;
  }
}

// ── connect（连接）───────────────────────────────────────────────────────────
Status LinuxTcpSocket::connect(const char* host, int port, int timeout_ms) {
  // 参数无效时提前返回
  if (host == nullptr || host[0] == '\0' || port <= 0 || port > 65535) {
    return Status::INVALID_ARGUMENT;
  }

  // 创建 TCP 套接字
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ < 0) {
    int err = errno;
    fd_ = -1;
    return errno_to_status(err);
  }

  // 设置为非阻塞模式，以便通过 poll() 实现连接超时
  int flags = fcntl(fd_, F_GETFL, 0);
  if (flags < 0) {
    int err = errno;
    ::close(fd_);
    fd_ = -1;
    return errno_to_status(err);
  }
  if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
    int err = errno;
    ::close(fd_);
    fd_ = -1;
    return errno_to_status(err);
  }

  // 通过 getaddrinfo 解析远程地址
  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;        // 仅 IPv4
  hints.ai_socktype = SOCK_STREAM;  // TCP 协议

  char port_str[8];
  std::snprintf(port_str, sizeof(port_str), "%d", port);

  struct addrinfo* result = nullptr;
  int gai_err = getaddrinfo(host, port_str, &hints, &result);
  if (gai_err != 0) {
    ::close(fd_);
    fd_ = -1;
    return Status::DNS_FAILURE;
  }

  // 遍历解析出的地址，直到成功连接
  Status status = Status::IO_ERROR;
  for (struct addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
    int rc = ::connect(fd_, ai->ai_addr, ai->ai_addrlen);
    if (rc == 0) {
      // 立即连接成功（非阻塞模式下虽罕见，但在回环/本地连接上可能发生）
      status = Status::OK;
      break;
    }

    if (errno != EINPROGRESS) {
      // 真正的连接失败——尝试下一个地址
      status = errno_to_status(errno);
      continue;
    }

    // 使用 poll() 等待连接完成
    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLOUT;

    int poll_rc = poll(&pfd, 1, timeout_ms > 0 ? timeout_ms : -1);
    if (poll_rc < 0) {
      // poll 被中断或失败
      status = errno == EINTR ? Status::IO_ERROR : errno_to_status(errno);
      continue;
    }
    if (poll_rc == 0) {
      // 超时，连接未完成
      status = Status::TIMEOUT;
      continue;
    }

    // poll() 报告套接字可写——检查 SO_ERROR 以区分成功连接与异步失败
    int so_error = 0;
    socklen_t optlen = sizeof(so_error);
    if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &so_error, &optlen) < 0) {
      status = errno_to_status(errno);
      continue;
    }

    if (so_error != 0) {
      status = errno_to_status(so_error);
      continue;
    }

    // 连接成功
    status = Status::OK;
    break;
  }

  freeaddrinfo(result);

  if (status != Status::OK) {
    ::close(fd_);
    fd_ = -1;
  }

  return status;
}

// ── send（发送）──────────────────────────────────────────────────────────────
Result<size_t> LinuxTcpSocket::send(const void* data, size_t len) {
  if (fd_ < 0) {
    return Result<size_t>::err(
        Error(Status::IO_ERROR, "send: socket not connected"));
  }

  const unsigned char* buf = static_cast<const unsigned char*>(data);
  size_t total_sent = 0;

  while (total_sent < len) {
    ssize_t n = ::send(fd_, buf + total_sent, len - total_sent, MSG_NOSIGNAL);
    if (n > 0) {
      total_sent += static_cast<size_t>(n);
      continue;
    }

    // n == 0 在流式套接字上不应发生，但按 EOF 处理
    if (n == 0) {
      break;
    }

    // n < 0 —— 处理错误
    int err = errno;

    if (err == EINTR) {
      // 被信号中断——立即重试
      continue;
    }

    if (err == EAGAIN || err == EWOULDBLOCK) {
      // 套接字缓冲区已满——等待 POLLOUT，然后重试
      struct pollfd pfd;
      pfd.fd = fd_;
      pfd.events = POLLOUT;

      int poll_rc = poll(&pfd, 1, -1);  // 无限等待
      if (poll_rc < 0) {
        if (errno == EINTR) {
          continue;
        }
        return Result<size_t>::err(
            Error(errno_to_status(errno), "send: poll failed"));
      }
      // 套接字再次可写——重试发送
      continue;
    }

    // 永久性错误
    return Result<size_t>::err(Error(errno_to_status(err), "send failed"));
  }

  return Result<size_t>::ok(total_sent);
}

// ── recv（接收）──────────────────────────────────────────────────────────────
Result<size_t> LinuxTcpSocket::recv(void* buf, size_t max_len) {
  if (fd_ < 0) {
    return Result<size_t>::err(
        Error(Status::IO_ERROR, "recv: socket not connected"));
  }

  // 使用 poll() 等待套接字变为可读
  struct pollfd pfd;
  pfd.fd = fd_;
  pfd.events = POLLIN;

  int poll_rc = poll(&pfd, 1, -1);  // 阻塞等待，直到数据到达
  if (poll_rc < 0) {
    if (errno == EINTR) {
      // 被信号中断时从头重试 poll
      return recv(buf, max_len);
    }
    return Result<size_t>::err(
        Error(errno_to_status(errno), "recv: poll failed"));
  }

  if (poll_rc == 0) {
    // 使用无限超时的 poll() 本不应超时，但仍妥善处理以防万一
    return Result<size_t>::err(Error(Status::TIMEOUT, "recv: poll returned 0"));
  }

  // 套接字可读——尝试接收数据
  ssize_t n = ::recv(fd_, buf, max_len, 0);
  if (n < 0) {
    int err = errno;
    if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK) {
      // 暂时性错误——从 poll 开始重试
      return recv(buf, max_len);
    }
    return Result<size_t>::err(Error(errno_to_status(err), "recv failed"));
  }

  // n == 0 表示正常 EOF（对端关闭了连接）
  return Result<size_t>::ok(static_cast<size_t>(n));
}

// ── close（关闭）─────────────────────────────────────────────────────────────
Status LinuxTcpSocket::close() {
  if (fd_ < 0) {
    return Status::OK;  // 已经关闭或从未打开
  }

  int fd = fd_;
  fd_ = -1;  // 在调用 ::close() 前标记为已关闭，确保幂等

  if (::close(fd) < 0) {
    return errno_to_status(errno);
  }

  return Status::OK;
}

// ============================================================================
// SocketFactory::create()  —— Linux 平台实现
// ============================================================================

// 静态方法
Result<Socket*> SocketFactory::create(const char* host, int port) {
  // 消除未使用参数的警告——|host| 和 |port| 通过 connect() 传入，不在构造时使用
  XNET_UNUSED(host);
  XNET_UNUSED(port);

  LinuxTcpSocket* sock = new (std::nothrow) LinuxTcpSocket();
  if (sock == nullptr) {
    return Result<Socket*>::err(Error(
        Status::OUT_OF_MEMORY, "SocketFactory::create: allocation failed"));
  }
  return Result<Socket*>::ok(static_cast<Socket*>(sock));
}

}  // namespace xnet
