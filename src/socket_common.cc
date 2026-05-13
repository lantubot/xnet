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

// SocketFactory 平台无关方法的实现。
//
// SocketFactory::create() 是平台相关的，位于 ports/ 目录下。
// SocketFactory::destroy() 是所有平台通用的实现，放在这里。

#include "xnet/socket.h"  // Socket, SocketFactory

namespace xnet {

// static
void SocketFactory::destroy(Socket* s) {
  if (s == nullptr) {
    return;
  }
  // 释放前先关闭套接字。根据 Socket 接口约定，close() 保证是幂等的，
  // 因此即使套接字已在外部被关闭，在此处调用也是安全的。
  (void)s->close();
  delete s;
}

}  // namespace xnet
