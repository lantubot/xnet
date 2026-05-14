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

#pragma once

/// @file socket_esp32.h
/// @brief ESP32 TCP socket abstraction atop LWIP.
///
/// Uses the BSD-style socket API provided by ESP-IDF / LWIP.  Timeouts are
/// implemented via SO_RCVTIMEO / SO_SNDTIMEO.  DNS resolution uses
/// lwip's built-in gethostbyname().
///
/// No STL dependencies.  C++20.

#include <esp_log.h>
#include <lwip/sockets.h>
#include <sys/socket.h>

#include "xnet/socket.h"

namespace xnet {

/// LWIP-backed TCP socket implementation for ESP32 (ESP-IDF).
///
/// Wraps a raw file descriptor using ESP-IDF's BSD-compatible socket API.
/// Timeouts are set via SO_RCVTIMEO / SO_SNDTIMEO socket options.
/// DNS resolution uses LWIP's gethostbyname().  Logging is performed
/// through the ESP-IDF log system (esp_log.h).  Implements the
/// xnet::Socket interface.
class Esp32TcpSocket : public Socket {
 public:
  /// Constructs an unconnected socket.  fd_ is initialised to -1.
  Esp32TcpSocket();

  /// Destructor calls close() internally if the socket is still open.
  ~Esp32TcpSocket() override;

  // ── Socket interface ───────────────────────────────────────────────────

  /// Connect to a remote host:port using an LWIP TCP socket.
  /// @param host  Null-terminated hostname or IP address.
  /// @param port  Port number in host byte order (1–65535).
  /// @param timeout_ms  Receive/send timeout in milliseconds applied via
  ///                    SO_RCVTIMEO and SO_SNDTIMEO; <= 0 means no timeout.
  /// @return Status::OK on success, or an error status on failure.
  Status connect(const char* host, int port, int timeout_ms) override;

  /// Send data over the connected socket.
  /// @param data  Pointer to the data buffer to send.
  /// @param len   Number of bytes to send.
  /// @return Result containing the number of bytes actually sent, or an error.
  Result<size_t> send(const void* data, size_t len) override;

  /// Receive data from the connected socket (blocking, with SO_RCVTIMEO).
  /// @param buf     Pointer to the receive buffer.
  /// @param max_len Maximum number of bytes to receive.
  /// @return Result containing the number of bytes received, or an error.
  Result<size_t> recv(void* buf, size_t max_len) override;

  /// Close the socket and release the file descriptor.
  /// @return Status::OK on success, or an error status on failure.
  Status close() override;

 private:
  int fd_;  ///< Socket file descriptor; -1 when not connected.

  // Non-copyable, non-movable.
  Esp32TcpSocket(const Esp32TcpSocket&) = delete;
  Esp32TcpSocket& operator=(const Esp32TcpSocket&) = delete;

  /// Map an errno value (from LWIP socket calls) to an xnet Status code.
  /// @param err  The errno value to translate.
  /// @return The corresponding Status enum value.
  static Status errno_to_status(int err);
};

}  // namespace xnet
