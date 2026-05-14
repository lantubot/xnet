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
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE CONDITIONS OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/** @file socket_linux.cc
 * @brief Implementation of LinuxTcpSocket — POSIX TCP socket for Linux.
 *
 * Uses non-blocking sockets (O_NONBLOCK via fcntl) with poll()-based I/O
 * multiplexing for connect/send/recv timeouts.  DNS resolution is performed
 * through getaddrinfo().  Error codes from errno are translated to the
 * platform-independent xnet::Status enum.
 *
 * No STL dependencies.  C++20.
 */

#include "socket_linux.h"

#include <arpa/inet.h>
#include <netdb.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <new>

#include "xnet/socket.h"

namespace xnet {

/** Default constructor — initialises fd_ to -1 (unconnected state). */
LinuxTcpSocket::LinuxTcpSocket() : fd_(-1) {}

/** Destructor — closes the socket if it is still open. */
LinuxTcpSocket::~LinuxTcpSocket() { close(); }

/** Translates a POSIX errno to an xnet::Status code.
 *
 * Handles connection errors (ECONNREFUSED, ECONNRESET, ETIMEDOUT),
 * resource errors (ENOMEM), protocol errors (EAFNOSUPPORT, etc.), and
 * transient errors (EAGAIN, EINTR).  All unrecognised codes map to
 * Status::IO_ERROR.
 *
 * @param err  The POSIX errno value to translate.
 * @return The corresponding xnet::Status.
 */
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

/** Connect to a remote host:port using a non-blocking POSIX socket.
 *
 * Creates a TCP socket, sets O_NONBLOCK, resolves the hostname via
 * getaddrinfo(), and issues a non-blocking connect().  If connect()
 * returns EINPROGRESS, poll() is used to wait for the connection to
 * complete or the specified timeout to elapse.  The socket error is
 * checked via getsockopt(SO_ERROR) after poll() returns.
 *
 * @param host       Null-terminated hostname or dotted-decimal IP.
 * @param port       Port number in host byte order (1–65535).
 * @param timeout_ms Max wait in milliseconds for connection completion;
 *                   <= 0 means wait indefinitely.
 * @return Status::OK on successful connection, or an error status.
 */
Status LinuxTcpSocket::connect(const char* host, int port, int timeout_ms) {
  if (host == nullptr || host[0] == '\0' || port <= 0 || port > 65535)
    return Status::INVALID_ARGUMENT;

  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ < 0) {
    int err = errno;
    fd_ = -1;
    return errno_to_status(err);
  }

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

  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  char port_str[8];
  std::snprintf(port_str, sizeof(port_str), "%d", port);

  struct addrinfo* result = nullptr;
  int gai_err = getaddrinfo(host, port_str, &hints, &result);
  if (gai_err != 0) {
    ::close(fd_);
    fd_ = -1;
    return Status::DNS_FAILURE;
  }

  Status status = Status::IO_ERROR;
  for (struct addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
    int rc = ::connect(fd_, ai->ai_addr, ai->ai_addrlen);
    if (rc == 0) {
      status = Status::OK;
      break;
    }

    if (errno != EINPROGRESS) {
      status = errno_to_status(errno);
      continue;
    }

    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLOUT;

    int poll_rc = poll(&pfd, 1, timeout_ms > 0 ? timeout_ms : -1);
    if (poll_rc < 0) {
      status = errno == EINTR ? Status::IO_ERROR : errno_to_status(errno);
      continue;
    }
    if (poll_rc == 0) {
      status = Status::TIMEOUT;
      continue;
    }

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

/** Send data over the connected socket.
 *
 * Loops until all bytes are transferred or a non-recoverable error occurs.
 * Uses MSG_NOSIGNAL to prevent SIGPIPE.  On EAGAIN/EWOULDBLOCK, waits
 * for the socket to become writable via poll() before retrying.
 *
 * @param data  Pointer to the buffer of data to send.
 * @param len   Number of bytes to send.
 * @return Result::ok with the number of bytes actually sent on success,
 *         or Result::err with an error status on failure.
 */
Result<size_t> LinuxTcpSocket::send(const void* data, size_t len) {
  if (fd_ < 0)
    return Result<size_t>::err(
        Error(Status::IO_ERROR, "send: socket not connected"));

  const unsigned char* buf = static_cast<const unsigned char*>(data);
  size_t total_sent = 0;

  while (total_sent < len) {
    ssize_t n = ::send(fd_, buf + total_sent, len - total_sent, MSG_NOSIGNAL);
    if (n > 0) {
      total_sent += static_cast<size_t>(n);
      continue;
    }
    if (n == 0) break;

    int err = errno;
    if (err == EINTR) continue;

    if (err == EAGAIN || err == EWOULDBLOCK) {
      struct pollfd pfd;
      pfd.fd = fd_;
      pfd.events = POLLOUT;
      int poll_rc = poll(&pfd, 1, -1);
      if (poll_rc < 0) {
        if (errno == EINTR) continue;
        return Result<size_t>::err(
            Error(errno_to_status(errno), "send: poll failed"));
      }
      continue;
    }
    return Result<size_t>::err(Error(errno_to_status(err), "send failed"));
  }
  return Result<size_t>::ok(total_sent);
}

/** Receive data from the connected socket (blocking).
 *
 * Uses poll() to wait for the socket to become readable.  Automatically
 * retries on EINTR, EAGAIN, and EWOULDBLOCK.  Returns 0 on connection
 * close by the peer.
 *
 * @param buf     Pointer to the buffer where received data will be stored.
 * @param max_len Maximum number of bytes to read into buf.
 * @return Result::ok with the number of bytes received on success,
 *         or Result::err with an error status on failure.
 */
Result<size_t> LinuxTcpSocket::recv(void* buf, size_t max_len) {
  if (fd_ < 0)
    return Result<size_t>::err(
        Error(Status::IO_ERROR, "recv: socket not connected"));

  struct pollfd pfd;
  pfd.fd = fd_;
  pfd.events = POLLIN;

  int poll_rc = poll(&pfd, 1, -1);
  if (poll_rc < 0) {
    if (errno == EINTR) return recv(buf, max_len);
    return Result<size_t>::err(
        Error(errno_to_status(errno), "recv: poll failed"));
  }
  if (poll_rc == 0)
    return Result<size_t>::err(Error(Status::TIMEOUT, "recv: poll returned 0"));

  ssize_t n = ::recv(fd_, buf, max_len, 0);
  if (n < 0) {
    int err = errno;
    if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK)
      return recv(buf, max_len);
    return Result<size_t>::err(Error(errno_to_status(err), "recv failed"));
  }
  return Result<size_t>::ok(static_cast<size_t>(n));
}

/** Close the socket and release the file descriptor.
 *
 * Sets fd_ to -1 before calling ::close() to prevent double-close
 * in concurrent scenarios.  Returns immediately if already closed.
 *
 * @return Status::OK on success, or an error status if ::close() fails.
 */
Status LinuxTcpSocket::close() {
  if (fd_ < 0) return Status::OK;
  int fd = fd_;
  fd_ = -1;
  if (::close(fd) < 0) return errno_to_status(errno);
  return Status::OK;
}

/** Factory method — creates a new LinuxTcpSocket instance.
 *
 * Allocates a LinuxTcpSocket on the heap using std::nothrow.  The
 * returned Socket* owns the allocation; the caller is responsible for
 * deleting it.
 *
 * @param host  Unused (host parameter reserved for future use).
 * @param port  Unused (port parameter reserved for future use).
 * @return Result::ok with a pointer to the new Socket on success,
 *         or Result::err with Status::OUT_OF_MEMORY on allocation failure.
 *
 * @note Linux implementation: simple heap allocation — no WSAStartup
 *       or other platform-specific initialisation needed.
 */
Result<Socket*> SocketFactory::create(const char* host, int port) {
  XNET_UNUSED(host);
  XNET_UNUSED(port);

  LinuxTcpSocket* sock = new (std::nothrow) LinuxTcpSocket();
  if (sock == nullptr)
    return Result<Socket*>::err(Error(
        Status::OUT_OF_MEMORY, "SocketFactory::create: allocation failed"));
  return Result<Socket*>::ok(static_cast<Socket*>(sock));
}

}  // namespace xnet
