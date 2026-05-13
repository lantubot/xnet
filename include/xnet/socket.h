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

#ifndef XNET_SOCKET_H_
#define XNET_SOCKET_H_

// TCP Socket 平台抽象层。
// 无 STL 依赖。C++20。不包含平台头文件。

#include <stddef.h>  // size_t

#include "xnet/error.h"

namespace xnet {

// SocketError — 套接字专用错误类型
struct SocketError : Error {
  using Error::Error;

  static inline SocketError connection_refused(const char* msg = nullptr) {
    return SocketError(Status::CONNECTION_REFUSED, msg);
  }

  static inline SocketError connection_reset(const char* msg = nullptr) {
    return SocketError(Status::CONNECTION_RESET, msg);
  }

  static inline SocketError timeout(const char* msg = nullptr) {
    return SocketError(Status::TIMEOUT, msg);
  }

  static inline SocketError dns_failure(const char* msg = nullptr) {
    return SocketError(Status::DNS_FAILURE, msg);
  }

  static inline SocketError not_found(const char* msg = nullptr) {
    return SocketError(Status::NOT_FOUND, msg);
  }
};

// Socket — TCP 套接字 I/O 抽象接口
class Socket {
 public:
  virtual ~Socket() = default;

  // 连接到远程 host:port。timeout_ms <= 0 表示无限阻塞。
  virtual Status connect(const char* host, int port, int timeout_ms) = 0;

  // 发送 len 字节。返回实际发送字节数或 Error。
  virtual Result<size_t> send(const void* data, size_t len) = 0;

  // 接收最多 max_len 字节到 buf。返回实际接收字节数或 Error。
  // 返回 0 表示对端已关闭（EOF）。
  virtual Result<size_t> recv(void* buf, size_t max_len) = 0;

  // 关闭套接字。多次调用安全（幂等）。
  virtual Status close() = 0;

 protected:
  Socket() = default;

 private:
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
};

// 平台相关 TCP 套接字前向声明
// 定义在各平台源文件：
//   Linux:   xnet/linux/tcp_socket.h
//   Windows: xnet/win/tcp_socket.h
//   ESP32:   xnet/esp32/tcp_socket.h

#ifdef __linux__
class LinuxTcpSocket;
#elif defined(_WIN32) || defined(_WIN64)
class WinTcpSocket;
#elif defined(ESP_PLATFORM) || defined(ESP32)
class Esp32TcpSocket;
#endif

// SocketFactory — 平台相关套接字创建的静态工厂
class SocketFactory {
 public:
  // 创建平台相关 TCP 套接字（未连接）。
  // host 为远程主机名（null 结尾），port 为 TCP 端口（主机字节序）。
  // 成功返回 Socket*，失败返回 Error。
  static Result<Socket*> create(const char* host, int port);

  // 销毁由 create() 创建的套接字。内部自动调用 close()。
  // 传入 nullptr 安全（无操作）。
  static void destroy(Socket* s);

 private:
  SocketFactory() = delete;
};

}  // namespace xnet

#endif  // XNET_SOCKET_H_
