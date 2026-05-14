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

/// @file socket_common.cc
/// @brief SocketFactory::destroy() 的平台无关实现。
///
/// 提供跨平台统一的 Socket 销毁逻辑。

#include "xnet/socket.h"

namespace xnet {

/// 销毁由 SocketFactory::create() 创建的套接字。
///
/// 内部自动调用 close() 后再 delete。传入 nullptr 时直接返回（幂等）。
/// @param s 要销毁的 Socket 指针，可为 nullptr。
void SocketFactory::destroy(Socket* s) {
  if (s == nullptr) return;
  XNET_UNUSED(s->close());
  delete s;
}

}  // namespace xnet
