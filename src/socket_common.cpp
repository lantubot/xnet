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

// Implementation of platform-independent SocketFactory methods.
//
// SocketFactory::create() is platform-specific and lives in ports/.
// SocketFactory::destroy() is common to all platforms and lives here.

#include "xnet/socket.h"  // Socket, SocketFactory

namespace xnet {

// static
void SocketFactory::destroy(Socket* s) {
  if (s == nullptr) {
    return;
  }
  // Close the socket before deallocating.  close() is guaranteed to be
  // idempotent per the Socket interface contract, so calling it here is
  // safe even if the socket was already closed externally.
  (void)s->close();
  delete s;
}

}  // namespace xnet
