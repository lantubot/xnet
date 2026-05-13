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

// ESP32 TCP socket abstraction atop LWIP.
//
// Uses the BSD-style socket API provided by ESP-IDF / LWIP.  Timeouts are
// implemented via SO_RCVTIMEO / SO_SNDTIMEO.  DNS resolution uses
// lwip's built-in gethostbyname().
//
// No STL dependencies.  C++20.

#include <esp_log.h>
#include <lwip/sockets.h>
#include <sys/socket.h>

#include "xnet/socket.h"

namespace xnet {

// ============================================================================
// Esp32TcpSocket — LWIP-backed TCP socket for ESP32
// ============================================================================
class Esp32TcpSocket : public Socket {
 public:
  // Create an unconnected socket.  fd_ is initialised to -1.
  Esp32TcpSocket();

  // Destructor calls close() internally if the socket is still open.
  ~Esp32TcpSocket() override;

  // ── Socket interface ───────────────────────────────────────────────────
  Status connect(const char* host, int port, int timeout_ms) override;
  Result<size_t> send(const void* data, size_t len) override;
  Result<size_t> recv(void* buf, size_t max_len) override;
  Status close() override;

 private:
  int fd_;  // socket descriptor, -1 when not connected

  // Non-copyable, non-movable.
  Esp32TcpSocket(const Esp32TcpSocket&) = delete;
  Esp32TcpSocket& operator=(const Esp32TcpSocket&) = delete;

  // Map errno (from socket/connect/send/recv) to an xnet Status code.
  static Status errno_to_status(int err);
};

}  // namespace xnet
