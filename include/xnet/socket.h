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

#ifndef XNET_SOCKET_H_
#define XNET_SOCKET_H_

// Platform abstraction for TCP socket communication.
//
// This header defines the abstract interface for sockets along with a
// factory that dispatches to platform-specific implementations
// (Linux, Windows, ESP32).
//
// No STL dependencies.  C++20.
// No platform headers are included.

#include <stddef.h>  // size_t

#include "xnet/error.h"

namespace xnet {

// ============================================================================
// SocketError — socket-specific error extending xnet::Error
// ============================================================================
struct SocketError : Error {
  // Inherit constructors for Status + optional message.
  using Error::Error;

  // Convenience: create a "connection refused" socket error.
  static SocketError connection_refused(const char* msg = nullptr) {
    return SocketError(Status::CONNECTION_REFUSED, msg);
  }

  // Convenience: create a "connection reset" socket error.
  static SocketError connection_reset(const char* msg = nullptr) {
    return SocketError(Status::CONNECTION_RESET, msg);
  }

  // Convenience: create a "timeout" socket error.
  static SocketError timeout(const char* msg = nullptr) {
    return SocketError(Status::TIMEOUT, msg);
  }

  // Convenience: create a "DNS failure" socket error.
  static SocketError dns_failure(const char* msg = nullptr) {
    return SocketError(Status::DNS_FAILURE, msg);
  }

  // Convenience: create a "not found" socket error (e.g. service unknown).
  static SocketError not_found(const char* msg = nullptr) {
    return SocketError(Status::NOT_FOUND, msg);
  }
};

// ============================================================================
// Socket — abstract interface for TCP socket I/O
// ============================================================================
class Socket {
 public:
  virtual ~Socket() = default;

  // Connect to a remote host:port with an optional timeout in milliseconds.
  // A timeout_ms <= 0 means block indefinitely (platform default behaviour).
  // Returns OK on success, or an error Status on failure.
  virtual Status connect(const char* host, int port, int timeout_ms) = 0;

  // Send |len| bytes from |data| over the socket.
  // Returns the number of bytes actually sent on success, or an Error
  // on failure.  A short send is not necessarily an error — the caller
  // must loop if the returned size is less than |len|.
  virtual Result<size_t> send(const void* data, size_t len) = 0;

  // Receive up to |max_len| bytes into |buf|.
  // Returns the number of bytes received on success, or an Error on
  // failure.  A return of 0 bytes indicates a clean connection close
  // (EOF) from the peer.
  virtual Result<size_t> recv(void* buf, size_t max_len) = 0;

  // Close the socket.  After close() returns, no further send/recv calls
  // are valid on this socket.  Calling close() multiple times is safe
  // (idempotent) for well-behaved implementations.
  virtual Status close() = 0;

 protected:
  // Default constructor accessible only to derived classes.
  Socket() = default;

 private:
  // Non-copyable, non-movable.
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
};

// ============================================================================
// Platform-specific TCP socket forward declarations
// ============================================================================
// These classes are defined in their respective platform source files:
//   - Linux:   xnet/linux/tcp_socket.h
//   - Windows: xnet/win/tcp_socket.h
//   - ESP32:   xnet/esp32/tcp_socket.h

#ifdef __linux__
class LinuxTcpSocket;
#elif defined(_WIN32) || defined(_WIN64)
class WinTcpSocket;
#elif defined(ESP_PLATFORM) || defined(ESP32)
class Esp32TcpSocket;
#endif

// ============================================================================
// SocketFactory — static factory for platform-specific socket creation
// ============================================================================
class SocketFactory {
 public:
  // Create a new platform-specific TCP socket.  The returned Socket* is
  // not connected; the caller must call connect() separately.
  //
  // |host| is the remote hostname or IP address (null-terminated).
  // |port| is the remote TCP port in host byte order.
  //
  // Returns a pointer to a new Socket on success, or an Error on failure
  // (e.g. out of memory, unsupported address family).
  static Result<Socket*> create(const char* host, int port);

  // Destroy a socket previously created by SocketFactory::create().
  // The socket is close()'d internally before deallocation.
  // Passing nullptr is safe (no-op).
  static void destroy(Socket* s);

 private:
  SocketFactory() = delete;
};

}  // namespace xnet

#endif  // XNET_SOCKET_H_
