// Copyright (c) 2026 XNet Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "socket_linux.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <new>          // std::nothrow

#include <arpa/inet.h>  // htons, inet_pton
#include <netdb.h>      // getaddrinfo, freeaddrinfo

#include "xnet/socket.h"

namespace xnet {

// ============================================================================
// LinuxTcpSocket
// ============================================================================

LinuxTcpSocket::LinuxTcpSocket() : fd_(-1) {}

LinuxTcpSocket::~LinuxTcpSocket() {
  close();
}

// ── errno_to_status ─────────────────────────────────────────────────────────
// static
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

// ── connect ─────────────────────────────────────────────────────────────────
Status LinuxTcpSocket::connect(const char* host, int port, int timeout_ms) {
  // Bail early on invalid arguments.
  if (host == nullptr || host[0] == '\0' || port <= 0 || port > 65535) {
    return Status::INVALID_ARGUMENT;
  }

  // Create a TCP socket.
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ < 0) {
    int err = errno;
    fd_ = -1;
    return errno_to_status(err);
  }

  // Set non-blocking so we can implement connect timeouts with poll().
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

  // Resolve the remote address via getaddrinfo.
  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family   = AF_INET;       // IPv4 only
  hints.ai_socktype = SOCK_STREAM;   // TCP

  char port_str[8];
  std::snprintf(port_str, sizeof(port_str), "%d", port);

  struct addrinfo* result = nullptr;
  int gai_err = getaddrinfo(host, port_str, &hints, &result);
  if (gai_err != 0) {
    ::close(fd_);
    fd_ = -1;
    return Status::DNS_FAILURE;
  }

  // Try each resolved address until one succeeds.
  Status status = Status::IO_ERROR;
  for (struct addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
    int rc = ::connect(fd_, ai->ai_addr, ai->ai_addrlen);
    if (rc == 0) {
      // Connected immediately (unlikely for non-blocking, but possible
      // on a loopback/localhost connection).
      status = Status::OK;
      break;
    }

    if (errno != EINPROGRESS) {
      // Genuine connection failure — try the next address.
      status = errno_to_status(errno);
      continue;
    }

    // Wait for the connection to complete with poll().
    struct pollfd pfd;
    pfd.fd     = fd_;
    pfd.events = POLLOUT;

    int poll_rc = poll(&pfd, 1, timeout_ms > 0 ? timeout_ms : -1);
    if (poll_rc < 0) {
      // poll interrupted or failed.
      status = errno == EINTR ? Status::IO_ERROR : errno_to_status(errno);
      continue;
    }
    if (poll_rc == 0) {
      // Timeout elapsed without connection completion.
      status = Status::TIMEOUT;
      continue;
    }

    // poll() reported the socket is writable — check SO_ERROR to
    // distinguish a successful connection from an asynchronous failure.
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

    // Connected successfully.
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

// ── send ────────────────────────────────────────────────────────────────────
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

    // n == 0 should never happen on a stream socket, but treat as EOF.
    if (n == 0) {
      break;
    }

    // n < 0  —  handle error.
    int err = errno;

    if (err == EINTR) {
      // Interrupted by signal — retry immediately.
      continue;
    }

    if (err == EAGAIN || err == EWOULDBLOCK) {
      // Socket buffer full — wait for POLLOUT, then retry.
      struct pollfd pfd;
      pfd.fd     = fd_;
      pfd.events = POLLOUT;

      int poll_rc = poll(&pfd, 1, -1);  // wait indefinitely
      if (poll_rc < 0) {
        if (errno == EINTR) {
          continue;
        }
        return Result<size_t>::err(
            Error(errno_to_status(errno), "send: poll failed"));
      }
      // Socket is writable again — retry the send.
      continue;
    }

    // Permanent error.
    return Result<size_t>::err(
        Error(errno_to_status(err), "send failed"));
  }

  return Result<size_t>::ok(total_sent);
}

// ── recv ────────────────────────────────────────────────────────────────────
Result<size_t> LinuxTcpSocket::recv(void* buf, size_t max_len) {
  if (fd_ < 0) {
    return Result<size_t>::err(
        Error(Status::IO_ERROR, "recv: socket not connected"));
  }

  // Wait for the socket to become readable using poll().
  struct pollfd pfd;
  pfd.fd     = fd_;
  pfd.events = POLLIN;

  int poll_rc = poll(&pfd, 1, -1);  // block indefinitely until data arrives
  if (poll_rc < 0) {
    if (errno == EINTR) {
      // Retry the poll from scratch on signal interruption.
      return recv(buf, max_len);
    }
    return Result<size_t>::err(
        Error(errno_to_status(errno), "recv: poll failed"));
  }

  if (poll_rc == 0) {
    // poll() with infinite timeout should never time out, but handle
    // gracefully just in case.
    return Result<size_t>::err(
        Error(Status::TIMEOUT, "recv: poll returned 0"));
  }

  // Socket is readable — attempt to receive.
  ssize_t n = ::recv(fd_, buf, max_len, 0);
  if (n < 0) {
    int err = errno;
    if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK) {
      // Transient — retry from the poll.
      return recv(buf, max_len);
    }
    return Result<size_t>::err(
        Error(errno_to_status(err), "recv failed"));
  }

  // n == 0 means clean EOF (peer closed the connection).
  return Result<size_t>::ok(static_cast<size_t>(n));
}

// ── close ───────────────────────────────────────────────────────────────────
Status LinuxTcpSocket::close() {
  if (fd_ < 0) {
    return Status::OK;  // already closed / never opened
  }

  int fd = fd_;
  fd_ = -1;  // mark closed before calling ::close() for idempotency

  if (::close(fd) < 0) {
    return errno_to_status(errno);
  }

  return Status::OK;
}

// ============================================================================
// SocketFactory::create()  —  Linux platform override
// ============================================================================

// static
Result<Socket*> SocketFactory::create(const char* host, int port) {
  // Silence unused-parameter warnings — |host| and |port| are passed
  // through to connect(), not used during construction.
  XNET_UNUSED(host);
  XNET_UNUSED(port);

  LinuxTcpSocket* sock = new (std::nothrow) LinuxTcpSocket();
  if (sock == nullptr) {
    return Result<Socket*>::err(
        Error(Status::OUT_OF_MEMORY,
              "SocketFactory::create: allocation failed"));
  }
  return Result<Socket*>::ok(static_cast<Socket*>(sock));
}

}  // namespace xnet
