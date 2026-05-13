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

#pragma once

// Linux TCP socket abstraction using POSIX sockets + poll().
//
// Non-blocking I/O via fcntl O_NONBLOCK.  Connect timeouts are implemented
// via non-blocking connect + poll().  Send/recv timeouts also use poll().
// DNS resolution uses getaddrinfo().
//
// No STL dependencies.  C++20.

#include <poll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>

#include "xnet/socket.h"

namespace xnet {

// ============================================================================
// LinuxTcpSocket — POSIX-backed TCP socket for Linux
// ============================================================================
class LinuxTcpSocket : public Socket {
 public:
  // Create an unconnected socket.  fd_ is initialised to -1.
  LinuxTcpSocket();

  // Destructor calls close() internally if the socket is still open.
  ~LinuxTcpSocket() override;

  // ── Socket interface ───────────────────────────────────────────────────
  Status connect(const char* host, int port, int timeout_ms) override;
  Result<size_t> send(const void* data, size_t len) override;
  Result<size_t> recv(void* buf, size_t max_len) override;
  Status close() override;

 private:
  int fd_;  // socket descriptor, -1 when not connected

  // Non-copyable, non-movable.
  LinuxTcpSocket(const LinuxTcpSocket&) = delete;
  LinuxTcpSocket& operator=(const LinuxTcpSocket&) = delete;

  // Map POSIX errno to an xnet Status code.
  static Status errno_to_status(int err);
};

}  // namespace xnet
