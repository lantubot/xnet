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

/** @file socket_windows.h
 * @brief Windows TCP socket abstraction over Winsock2.
 *
 * Uses the Winsock2 API (ws2_32).  Connect timeouts are implemented via
 * non-blocking connect + select().  Send/recv timeouts also use select().
 * DNS resolution uses getaddrinfo().
 *
 * No STL dependencies.  C++20.
 * Link with ws2_32.lib (handled in CMakeLists.txt).
 */

#include <winsock2.h>
#include <ws2tcpip.h>  // getaddrinfo, freeaddrinfo

#include "xnet/socket.h"

namespace xnet {

/** Winsock2-backed TCP socket implementation for Windows.
 *
 * Wraps a SOCKET handle with non-blocking I/O (via ioctlsocket),
 * select()-based timeouts, and getaddrinfo() DNS resolution.  Ensures
 * WSAStartup is called before any socket operations.  Implements the
 * xnet::Socket interface.
 */
class WinTcpSocket : public Socket {
 public:
  /** Constructs an unconnected socket.  fd_ is initialised to INVALID_SOCKET.
   */
  WinTcpSocket();

  /** Destructor calls close() internally if the socket is still open. */
  ~WinTcpSocket() override;

  // ── Socket interface ───────────────────────────────────────────────────

  /** Connect to a remote host:port.
   * @param host  Null-terminated hostname or IP address.
   * @param port  Port number in host byte order (1–65535).
   * @param timeout_ms  Connection timeout in milliseconds; <= 0 means no
   *                    timeout (blocking).
   * @return Status::OK on success, or an error status on failure.
   */
  Status connect(const char* host, int port, int timeout_ms) override;

  /** Send data over the connected socket.
   * @param data  Pointer to the data buffer to send.
   * @param len   Number of bytes to send.
   * @return Result containing the number of bytes actually sent, or an error.
   */
  Result<size_t> send(const void* data, size_t len) override;

  /** Receive data from the connected socket (blocking, via select()).
   * @param buf     Pointer to the receive buffer.
   * @param max_len Maximum number of bytes to receive.
   * @return Result containing the number of bytes received, or an error.
   */
  Result<size_t> recv(void* buf, size_t max_len) override;

  /** Close the socket, issue shutdown(SD_BOTH), and release the SOCKET handle.
   * @return Status::OK on success, or an error status on failure.
   */
  Status close() override;

 private:
  SOCKET fd_; /**< Winsock socket handle; INVALID_SOCKET when not connected. */

  // Non-copyable, non-movable.
  WinTcpSocket(const WinTcpSocket&) = delete;
  WinTcpSocket& operator=(const WinTcpSocket&) = delete;

  /** Map a Winsock error code (from WSAGetLastError) to an xnet Status code.
   * @param wsa_err  The Winsock error code to translate.
   * @return The corresponding Status enum value.
   */
  static Status wsa_error_to_status(int wsa_err);
};

}  // namespace xnet
