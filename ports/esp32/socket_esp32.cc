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

#include "socket_esp32.h"

#include <cerrno>
#include <cstring>   // memset, memcpy
#include <netdb.h>   // gethostbyname
#include <arpa/inet.h>  // inet_addr, htons

#include "xnet/socket.h"

namespace xnet {

// Logging tag used with ESP_LOG* macros.
static const char* const TAG = "xnet.esp32";

// ============================================================================
// Esp32TcpSocket
// ============================================================================

Esp32TcpSocket::Esp32TcpSocket() : fd_(-1) {}

Esp32TcpSocket::~Esp32TcpSocket() {
  close();
}

// ── errno_to_status ─────────────────────────────────────────────────────────
// static
Status Esp32TcpSocket::errno_to_status(int err) {
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
Status Esp32TcpSocket::connect(const char* host, int port, int timeout_ms) {
  // Bail early on invalid arguments.
  if (host == nullptr || host[0] == '\0' || port <= 0 || port > 65535) {
    ESP_LOGE(TAG, "connect: invalid args (host=%p, port=%d)", (const void*)host, port);
    return Status::INVALID_ARGUMENT;
  }

  // Create a TCP socket.
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ < 0) {
    int err = errno;
    ESP_LOGE(TAG, "socket() failed: errno=%d", err);
    return errno_to_status(err);
  }

  // Set receive and send timeouts if requested.
  if (timeout_ms > 0) {
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    if (setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
      ESP_LOGW(TAG, "setsockopt(SO_RCVTIMEO) failed: errno=%d", errno);
    }
    if (setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
      ESP_LOGW(TAG, "setsockopt(SO_SNDTIMEO) failed: errno=%d", errno);
    }
  }

  // Resolve the remote address.
  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port   = htons(static_cast<uint16_t>(port));

  // Try interpreting |host| as a dotted-quad IP address first.
  addr.sin_addr.s_addr = inet_addr(host);
  if (addr.sin_addr.s_addr == INADDR_NONE) {
    // Not a raw IP — perform DNS resolution via lwip's gethostbyname().
    struct hostent* he = gethostbyname(host);
    if (he == nullptr) {
      int h_err = h_errno;
      ESP_LOGE(TAG, "gethostbyname(\"%s\") failed: h_errno=%d", host, h_err);
      ::close(fd_);
      fd_ = -1;
      return Status::DNS_FAILURE;
    }
    // Copy the first resolved address.
    if (he->h_length > static_cast<int>(sizeof(addr.sin_addr))) {
      he->h_length = sizeof(addr.sin_addr);
    }
    std::memcpy(&addr.sin_addr, he->h_addr, static_cast<size_t>(he->h_length));
  }

  // Initiate the TCP connection.
  if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    int err = errno;
    ESP_LOGE(TAG, "connect(\"%s\", %d) failed: errno=%d", host, port, err);
    ::close(fd_);
    fd_ = -1;
    return errno_to_status(err);
  }

  ESP_LOGD(TAG, "connected to %s:%d (fd=%d)", host, port, fd_);
  return Status::OK;
}

// ── send ────────────────────────────────────────────────────────────────────
Result<size_t> Esp32TcpSocket::send(const void* data, size_t len) {
  if (fd_ < 0) {
    return Result<size_t>::err(Error(Status::IO_ERROR, "send: socket not connected"));
  }

  const uint8_t* buf = static_cast<const uint8_t*>(data);
  size_t total_sent = 0;

  while (total_sent < len) {
    ssize_t n = ::send(fd_, buf + total_sent, len - total_sent, 0);
    if (n < 0) {
      int err = errno;
      // EAGAIN / EWOULDBLOCK with SO_SNDTIMEO means timeout — return what
      // we sent so far so the caller can decide whether to retry.
      if ((err == EAGAIN || err == EWOULDBLOCK) && total_sent > 0) {
        break;
      }
      ESP_LOGE(TAG, "send() failed: errno=%d", err);
      return Result<size_t>::err(Error(errno_to_status(err), "send failed"));
    }
    if (n == 0) {
      // send() should never return 0 on a connected stream socket, but
      // treat it as EOF to be safe.
      break;
    }
    total_sent += static_cast<size_t>(n);
  }

  return Result<size_t>::ok(total_sent);
}

// ── recv ────────────────────────────────────────────────────────────────────
Result<size_t> Esp32TcpSocket::recv(void* buf, size_t max_len) {
  if (fd_ < 0) {
    return Result<size_t>::err(Error(Status::IO_ERROR, "recv: socket not connected"));
  }

  ssize_t n = ::recv(fd_, buf, max_len, 0);
  if (n < 0) {
    int err = errno;
    ESP_LOGE(TAG, "recv() failed: errno=%d", err);
    return Result<size_t>::err(Error(errno_to_status(err), "recv failed"));
  }

  // n == 0 means clean EOF (peer closed the connection).
  return Result<size_t>::ok(static_cast<size_t>(n));
}

// ── close ───────────────────────────────────────────────────────────────────
Status Esp32TcpSocket::close() {
  if (fd_ < 0) {
    return Status::OK;  // already closed / never opened
  }

  int fd = fd_;
  fd_ = -1;  // mark closed before calling ::close() for idempotency

  if (::close(fd) < 0) {
    int err = errno;
    ESP_LOGW(TAG, "close(fd=%d) failed: errno=%d", fd, err);
    return errno_to_status(err);
  }

  ESP_LOGD(TAG, "closed fd=%d", fd);
  return Status::OK;
}

// ============================================================================
// SocketFactory::create()  —  ESP32 platform override
// ============================================================================

// static
Result<Socket*> SocketFactory::create(const char* host, int port) {
  // Silence unused-parameter warnings — |host| and |port| are passed
  // through to connect(), not used during construction.
  XNET_UNUSED(host);
  XNET_UNUSED(port);

  Esp32TcpSocket* sock = new (std::nothrow) Esp32TcpSocket();
  if (sock == nullptr) {
    return Result<Socket*>::err(Error(Status::OUT_OF_MEMORY,
                                       "SocketFactory::create: allocation failed"));
  }
  return Result<Socket*>::ok(static_cast<Socket*>(sock));
}

}  // namespace xnet
