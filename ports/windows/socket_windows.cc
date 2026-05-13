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

// Windows 平台 WinTcpSocket 和 SocketFactory::create() 的实现
//
// 所有套接字操作均使用 Winsock2。连接超时通过非阻塞 connect() + select() 实现。
// 接收超时使用 select() 在读取 fd_set 上实现。
// 发送使用部分写入循环，直到所有字节发送完毕或发生错误。

#include "socket_windows.h"

#include <cstring>  // memset（内存操作）

namespace xnet {

// ============================================================================
// WSAStartup 管理
// ============================================================================
// 记录 WSAStartup 是否已成功调用。对于单线程使用，静态标志足够；
// 多线程调用者应确保在使用前调用 WSAStartup。
// 我们使用 InterlockedExchange 操作一个 LONG 类型以提供基本的线程安全，无需 STL
// 原子操作。

static LONG g_wsa_started = 0;

static Status EnsureWsaStarted() {
  if (g_wsa_started) {
    return Status::OK;
  }

  WSADATA wsa_data;
  int rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (rc != 0) {
    return Status::IO_ERROR;
  }

  InterlockedExchange(&g_wsa_started, 1);
  return Status::OK;
}

// ============================================================================
// WinTcpSocket 构造 / 析构
// ============================================================================

WinTcpSocket::WinTcpSocket() : fd_(INVALID_SOCKET) {}

WinTcpSocket::~WinTcpSocket() {
  // 按照 Socket 契约，close() 是幂等的
  (void)close();
}

// ============================================================================
// WSA 错误码 -> xnet 状态码映射
// ============================================================================

// 静态方法
Status WinTcpSocket::wsa_error_to_status(int wsa_err) {
  switch (wsa_err) {
    case WSAETIMEDOUT:
      return Status::TIMEOUT;
    case WSAECONNREFUSED:
      return Status::CONNECTION_REFUSED;
    case WSAECONNRESET:
    case WSAECONNABORTED:
      return Status::CONNECTION_RESET;
    case WSAHOST_NOT_FOUND:
    case WSANO_DATA:
    case WSANO_RECOVERY:
      return Status::DNS_FAILURE;
    case WSAEADDRNOTAVAIL:
    case WSAENETUNREACH:
    case WSAEHOSTUNREACH:
      return Status::UNKNOWN;
    case WSAEINVAL:
    case WSAEAFNOSUPPORT:
      return Status::UNSUPPORTED_PROTOCOL;
    case WSAENOBUFS:
    case WSA_NOT_ENOUGH_MEMORY:
      return Status::OUT_OF_MEMORY;
    default:
      return Status::IO_ERROR;
  }
}

// ============================================================================
// 端口转字符串辅助函数（无 STL）
// ============================================================================
// 将 |port| 的十进制表示写入 |buf|（至少 6 字节）。
// 返回指向 |buf| 的指针（跳过前导零）。
static const char* PortToString(int port, char* buf, size_t buf_size) {
  (void)buf_size;
  // 反向写入数字
  char tmp[6];  // max "65535\0" = 6 chars
  size_t idx = 0;
  if (port == 0) {
    tmp[idx++] = '0';
  } else {
    int p = port;
    while (p > 0 && idx < sizeof(tmp) - 1) {
      tmp[idx++] = static_cast<char>('0' + (p % 10));
      p /= 10;
    }
  }

  // Reverse into output buffer.
  size_t out = 0;
  while (idx > 0) {
    --idx;
    buf[out++] = tmp[idx];
  }
  buf[out] = '\0';
  return buf;
}

// ============================================================================
// connect()  —  non-blocking connect + select() timeout
// ============================================================================

Status WinTcpSocket::connect(const char* host, int port, int timeout_ms) {
  // --- 1. Ensure WSA is initialised -----------------------------------------
  Status ws = EnsureWsaStarted();
  if (ws != Status::OK) {
    return ws;
  }

  // --- 2. Close any previous connection --------------------------------------
  if (fd_ != INVALID_SOCKET) {
    (void)close();
  }

  // --- 3. Convert port to string for getaddrinfo -----------------------------
  char port_str[8];
  PortToString(port, port_str, sizeof(port_str));

  // --- 4. DNS resolution -----------------------------------------------------
  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;  // IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo* addr_info = nullptr;
  int rc = getaddrinfo(host, port_str, &hints, &addr_info);
  if (rc != 0 || addr_info == nullptr) {
    return Status::DNS_FAILURE;
  }

  // --- 5. Create socket ------------------------------------------------------
  SOCKET s = socket(addr_info->ai_family, addr_info->ai_socktype,
                    addr_info->ai_protocol);
  if (s == INVALID_SOCKET) {
    int wsa_err = WSAGetLastError();
    freeaddrinfo(addr_info);
    return wsa_error_to_status(wsa_err);
  }

  // --- 6. Set non-blocking for timed connect ---------------------------------
  u_long nonblock = 1;
  rc = ioctlsocket(s, FIONBIO, &nonblock);
  if (rc != 0) {
    int wsa_err = WSAGetLastError();
    closesocket(s);
    freeaddrinfo(addr_info);
    return wsa_error_to_status(wsa_err);
  }

  // --- 7. Initiate the connection (will return WSAEWOULDBLOCK) ---------------
  rc = ::connect(s, addr_info->ai_addr, (int)addr_info->ai_addrlen);
  if (rc != 0) {
    int wsa_err = WSAGetLastError();
    if (wsa_err != WSAEWOULDBLOCK && wsa_err != WSAEINPROGRESS) {
      closesocket(s);
      freeaddrinfo(addr_info);
      return wsa_error_to_status(wsa_err);
    }
  }

  // --- 8. Wait for connection to complete via select() -----------------------
  fd_set write_fds;
  FD_ZERO(&write_fds);
  FD_SET(s, &write_fds);

  fd_set except_fds;
  FD_ZERO(&except_fds);
  FD_SET(s, &except_fds);

  // Prepare timeout.
  struct timeval tv;
  struct timeval* ptv = nullptr;
  if (timeout_ms > 0) {
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ptv = &tv;
  }

  rc = select(0, nullptr, &write_fds, &except_fds, ptv);
  if (rc == 0) {
    // Timeout — connection did not complete in time.
    closesocket(s);
    freeaddrinfo(addr_info);
    return Status::TIMEOUT;
  }
  if (rc == SOCKET_ERROR) {
    int wsa_err = WSAGetLastError();
    closesocket(s);
    freeaddrinfo(addr_info);
    return wsa_error_to_status(wsa_err);
  }

  // Check whether the connection succeeded (SO_ERROR).
  if (FD_ISSET(s, &except_fds)) {
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen);
    closesocket(s);
    freeaddrinfo(addr_info);
    return wsa_error_to_status(optval ? optval : WSAECONNREFUSED);
  }

  // --- 9. Verify connection succeeded via SO_ERROR ---------------------------
  int optval = 0;
  socklen_t optlen = sizeof(optval);
  rc = getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen);
  if (rc != 0 || optval != 0) {
    int err = (rc != 0) ? WSAGetLastError() : optval;
    closesocket(s);
    freeaddrinfo(addr_info);
    return wsa_error_to_status(err);
  }

  // --- 10. Restore blocking mode ---------------------------------------------
  u_long block = 0;
  ioctlsocket(s, FIONBIO, &block);

  // --- 11. Store the socket and clean up -------------------------------------
  fd_ = s;
  freeaddrinfo(addr_info);
  return Status::OK;
}

// ============================================================================
// send()  —  partial-write loop
// ============================================================================

Result<size_t> WinTcpSocket::send(const void* data, size_t len) {
  if (fd_ == INVALID_SOCKET) {
    return Result<size_t>::err(Error(Status::IO_ERROR, "socket not connected"));
  }
  if (data == nullptr || len == 0) {
    return Result<size_t>::ok(static_cast<size_t>(0));
  }

  const char* buf = static_cast<const char*>(data);
  size_t total_sent = 0;

  while (total_sent < len) {
    int sent =
        ::send(fd_, buf + total_sent, static_cast<int>(len - total_sent), 0);
    if (sent == SOCKET_ERROR) {
      int wsa_err = WSAGetLastError();
      // WSAEWOULDBLOCK should not occur in blocking mode, but handle it
      // gracefully by yielding the remainder as a partial send.
      if (wsa_err == WSAEWOULDBLOCK || wsa_err == WSAEINTR) {
        continue;
      }
      // Report the bytes sent so far alongside the error.
      if (total_sent > 0) {
        return Result<size_t>::ok(total_sent);
      }
      return Result<size_t>::err(
          Error(wsa_error_to_status(wsa_err), "send failed"));
    }
    total_sent += static_cast<size_t>(sent);
  }

  return Result<size_t>::ok(total_sent);
}

// ============================================================================
// recv()  —  select() timeout then recv()
// ============================================================================

Result<size_t> WinTcpSocket::recv(void* buf, size_t max_len) {
  if (fd_ == INVALID_SOCKET) {
    return Result<size_t>::err(Error(Status::IO_ERROR, "socket not connected"));
  }
  if (buf == nullptr || max_len == 0) {
    return Result<size_t>::err(
        Error(Status::INVALID_ARGUMENT, "invalid recv arguments"));
  }

  // --- 1. Use select() to wait for data with a default timeout ---------------
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(fd_, &read_fds);

  // Default recv timeout: 30 seconds.
  const int kRecvTimeoutMs = 30000;
  struct timeval tv;
  tv.tv_sec = kRecvTimeoutMs / 1000;
  tv.tv_usec = (kRecvTimeoutMs % 1000) * 1000;

  int rc = select(0, &read_fds, nullptr, nullptr, &tv);
  if (rc == 0) {
    // Timeout — no data arrived within the window.
    return Result<size_t>::err(SocketError::timeout("recv timed out"));
  }
  if (rc == SOCKET_ERROR) {
    int wsa_err = WSAGetLastError();
    return Result<size_t>::err(
        Error(wsa_error_to_status(wsa_err), "select failed in recv"));
  }

  // --- 2. Data is available — receive it -------------------------------------
  int nread =
      ::recv(fd_, static_cast<char*>(buf), static_cast<int>(max_len), 0);
  if (nread == SOCKET_ERROR) {
    int wsa_err = WSAGetLastError();
    // WSAEWOULDBLOCK after select() indicates a race — retry is reasonable.
    if (wsa_err == WSAEWOULDBLOCK || wsa_err == WSAEINTR) {
      return Result<size_t>::ok(static_cast<size_t>(0));
    }
    return Result<size_t>::err(
        Error(wsa_error_to_status(wsa_err), "recv failed"));
  }

  // nread == 0 indicates clean EOF.
  return Result<size_t>::ok(static_cast<size_t>(nread));
}

// ============================================================================
// close()  —  closesocket()
// ============================================================================

Status WinTcpSocket::close() {
  if (fd_ == INVALID_SOCKET) {
    return Status::OK;  // already closed / never opened
  }

  // Graceful shutdown: signal that we are done sending.
  ::shutdown(fd_, SD_BOTH);

  if (closesocket(fd_) == SOCKET_ERROR) {
    int wsa_err = WSAGetLastError();
    fd_ = INVALID_SOCKET;
    return wsa_error_to_status(wsa_err);
  }

  fd_ = INVALID_SOCKET;
  return Status::OK;
}

// ============================================================================
// SocketFactory::create()  —  Windows implementation
// ============================================================================

// static
Result<Socket*> SocketFactory::create(const char* host, int port) {
  (void)host;
  (void)port;

  // Ensure WSA is started before we hand the socket to the caller.
  Status ws = EnsureWsaStarted();
  if (ws != Status::OK) {
    return Result<Socket*>::err(Error(ws, "WSAStartup failed"));
  }

  WinTcpSocket* sock = new WinTcpSocket();
  if (sock == nullptr) {
    return Result<Socket*>::err(
        Error(Status::OUT_OF_MEMORY, "failed to allocate WinTcpSocket"));
  }

  return Result<Socket*>::ok(static_cast<Socket*>(sock));
}

}  // namespace xnet
