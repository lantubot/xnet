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

/// @file socket_esp32.cc
/// @brief Implementation of Esp32TcpSocket — LWIP TCP socket for ESP32.
///
/// Uses ESP-IDF's BSD-compatible socket API atop the LWIP stack.  Timeouts
/// are applied via SO_RCVTIMEO / SO_SNDTIMEO setsockopt() rather than
/// poll()/select(), since the LWIP socket layer supports these options
/// natively.  DNS resolution uses LWIP's gethostbyname().  Error codes
/// from errno (set by LWIP) are translated to xnet::Status.
///
/// Logging uses the ESP-IDF log system (esp_log.h) with tag "xnet.esp32".
///
/// No STL dependencies.  C++20.

#include "socket_esp32.h"

#include <arpa/inet.h>
#include <netdb.h>

#include <cerrno>
#include <cstring>

#include "xnet/socket.h"

namespace xnet {

/// Logging tag used for ESP-IDF log output from this module.
static const char* const TAG = "xnet.esp32";

/// Default constructor — initialises fd_ to -1 (unconnected state).
Esp32TcpSocket::Esp32TcpSocket() : fd_(-1) {}

/// Destructor — closes the socket if it is still open.
Esp32TcpSocket::~Esp32TcpSocket() { close(); }

/// Translates an errno (from LWIP socket operations) to an xnet::Status.
///
/// Handles connection errors (ECONNREFUSED, ECONNRESET, ETIMEDOUT),
/// resource errors (ENOMEM), protocol errors (EAFNOSUPPORT, etc.), and
/// transient errors (EAGAIN, EINTR).  All unrecognised codes map to
/// Status::IO_ERROR.
///
/// @param err  The errno value to translate.
/// @return The corresponding xnet::Status.
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

/// Connect to a remote host:port using an LWIP TCP socket.
///
/// Creates a TCP socket, optionally sets SO_RCVTIMEO and SO_SNDTIMEO for
/// I/O timeouts, resolves the hostname via inet_addr() (for dotted-decimal)
/// or gethostbyname() (for hostnames), and issues a blocking connect().
/// Logs errors via the ESP-IDF log system on failure.
///
/// @param host       Null-terminated hostname or dotted-decimal IP.
/// @param port       Port number in host byte order (1–65535).
/// @param timeout_ms Receive/send timeout in milliseconds applied via
///                   SO_RCVTIMEO / SO_SNDTIMEO; <= 0 means no timeout.
/// @return Status::OK on successful connection, or an error status.
Status Esp32TcpSocket::connect(const char* host, int port, int timeout_ms) {
  if (host == nullptr || host[0] == '\0' || port <= 0 || port > 65535) {
    ESP_LOGE(TAG, "connect: invalid args (host=%p, port=%d)", (const void*)host,
             port);
    return Status::INVALID_ARGUMENT;
  }

  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ < 0) {
    int err = errno;
    ESP_LOGE(TAG, "socket() failed: errno=%d", err);
    return errno_to_status(err);
  }

  if (timeout_ms > 0) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
      ESP_LOGW(TAG, "setsockopt(SO_RCVTIMEO) failed: errno=%d", errno);
    if (setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
      ESP_LOGW(TAG, "setsockopt(SO_SNDTIMEO) failed: errno=%d", errno);
  }

  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));

  addr.sin_addr.s_addr = inet_addr(host);
  if (addr.sin_addr.s_addr == INADDR_NONE) {
    struct hostent* he = gethostbyname(host);
    if (he == nullptr) {
      int h_err = h_errno;
      ESP_LOGE(TAG, "gethostbyname(\"%s\") failed: h_errno=%d", host, h_err);
      ::close(fd_);
      fd_ = -1;
      return Status::DNS_FAILURE;
    }
    if (he->h_length > static_cast<int>(sizeof(addr.sin_addr)))
      he->h_length = sizeof(addr.sin_addr);
    std::memcpy(&addr.sin_addr, he->h_addr, static_cast<size_t>(he->h_length));
  }

  if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) <
      0) {
    int err = errno;
    ESP_LOGE(TAG, "connect(\"%s\", %d) failed: errno=%d", host, port, err);
    ::close(fd_);
    fd_ = -1;
    return errno_to_status(err);
  }

  ESP_LOGD(TAG, "connected to %s:%d (fd=%d)", host, port, fd_);
  return Status::OK;
}

/// Send data over the connected ESP32 socket.
///
/// Loops until all bytes are transferred or a non-recoverable error occurs.
/// On EAGAIN/EWOULDBLOCK with partial data sent, returns the partial count.
/// Logs errors via the ESP-IDF log system on failure.
///
/// @param data  Pointer to the buffer of data to send.
/// @param len   Number of bytes to send.
/// @return Result::ok with the number of bytes actually sent on success,
///         or Result::err with an error status on failure.
Result<size_t> Esp32TcpSocket::send(const void* data, size_t len) {
  if (fd_ < 0)
    return Result<size_t>::err(
        Error(Status::IO_ERROR, "send: socket not connected"));

  const uint8_t* buf = static_cast<const uint8_t*>(data);
  size_t total_sent = 0;

  while (total_sent < len) {
    ssize_t n = ::send(fd_, buf + total_sent, len - total_sent, 0);
    if (n < 0) {
      int err = errno;
      if ((err == EAGAIN || err == EWOULDBLOCK) && total_sent > 0) break;
      ESP_LOGE(TAG, "send() failed: errno=%d", err);
      return Result<size_t>::err(Error(errno_to_status(err), "send failed"));
    }
    if (n == 0) break;
    total_sent += static_cast<size_t>(n);
  }
  return Result<size_t>::ok(total_sent);
}

/// Receive data from the connected ESP32 socket (blocking).
///
/// Uses SO_RCVTIMEO (set during connect()) for timeout control.  Returns 0
/// on connection close by the peer.  Logs errors via the ESP-IDF log system.
///
/// @param buf     Pointer to the buffer where received data will be stored.
/// @param max_len Maximum number of bytes to read into buf.
/// @return Result::ok with the number of bytes received on success,
///         or Result::err with an error status on failure.
Result<size_t> Esp32TcpSocket::recv(void* buf, size_t max_len) {
  if (fd_ < 0)
    return Result<size_t>::err(
        Error(Status::IO_ERROR, "recv: socket not connected"));

  ssize_t n = ::recv(fd_, buf, max_len, 0);
  if (n < 0) {
    int err = errno;
    ESP_LOGE(TAG, "recv() failed: errno=%d", err);
    return Result<size_t>::err(Error(errno_to_status(err), "recv failed"));
  }
  return Result<size_t>::ok(static_cast<size_t>(n));
}

/// Close the ESP32 socket and release the file descriptor.
///
/// Sets fd_ to -1 before calling ::close() to prevent double-close.
/// Logs warnings if the close fails.  Returns immediately if already closed.
///
/// @return Status::OK on success, or an error status if ::close() fails.
Status Esp32TcpSocket::close() {
  if (fd_ < 0) return Status::OK;
  int fd = fd_;
  fd_ = -1;
  if (::close(fd) < 0) {
    int err = errno;
    ESP_LOGW(TAG, "close(fd=%d) failed: errno=%d", fd, err);
    return errno_to_status(err);
  }
  ESP_LOGD(TAG, "closed fd=%d", fd);
  return Status::OK;
}

/// Factory method — creates a new Esp32TcpSocket instance.
///
/// Allocates an Esp32TcpSocket on the heap using std::nothrow.  The
/// returned Socket* owns the allocation; the caller is responsible for
/// deleting it.
///
/// @param host  Unused (host parameter reserved for future use).
/// @param port  Unused (port parameter reserved for future use).
/// @return Result::ok with a pointer to the new Socket on success,
///         or Result::err with Status::OUT_OF_MEMORY on allocation failure.
///
/// @note ESP32 implementation: simple heap allocation using std::nothrow
///       — no platform-specific initialisation required before allocation.
Result<Socket*> SocketFactory::create(const char* host, int port) {
  XNET_UNUSED(host);
  XNET_UNUSED(port);

  Esp32TcpSocket* sock = new (std::nothrow) Esp32TcpSocket();
  if (sock == nullptr)
    return Result<Socket*>::err(Error(
        Status::OUT_OF_MEMORY, "SocketFactory::create: allocation failed"));
  return Result<Socket*>::ok(static_cast<Socket*>(sock));
}

}  // namespace xnet
