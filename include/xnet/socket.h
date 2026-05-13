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

// TCP Socket 通信的平台抽象层。
//
// 本头文件定义了 Socket 的抽象接口以及一个工厂类，
// 工厂类负责将创建请求分发到具体平台的实现（Linux、Windows、ESP32）。
//
// 无 STL 依赖。C++20。
// 不包含任何平台头文件。

#include <stddef.h>  // size_t

#include "xnet/error.h"

namespace xnet {

// ============================================================================
// SocketError — 套接字专用错误类型，继承自 xnet::Error
// ============================================================================
struct SocketError : Error {
  // 继承 Error 的构造函数（Status + 可选消息）。
  using Error::Error;

  // 便捷方法：创建"连接被拒绝"的套接字错误。
  static inline SocketError connection_refused(const char* msg = nullptr) {
    return SocketError(Status::CONNECTION_REFUSED, msg);
  }

  // 便捷方法：创建"连接被重置"的套接字错误。
  static inline SocketError connection_reset(const char* msg = nullptr) {
    return SocketError(Status::CONNECTION_RESET, msg);
  }

  // 便捷方法：创建"超时"的套接字错误。
  static inline SocketError timeout(const char* msg = nullptr) {
    return SocketError(Status::TIMEOUT, msg);
  }

  // 便捷方法：创建"DNS 解析失败"的套接字错误。
  static inline SocketError dns_failure(const char* msg = nullptr) {
    return SocketError(Status::DNS_FAILURE, msg);
  }

  // 便捷方法：创建"未找到"的套接字错误（例如服务未知）。
  static inline SocketError not_found(const char* msg = nullptr) {
    return SocketError(Status::NOT_FOUND, msg);
  }
};

// ============================================================================
// Socket — TCP 套接字 I/O 的抽象接口
// ============================================================================
class Socket {
 public:
  virtual ~Socket() = default;

  // 连接到远程 host:port，支持可选的超时时间（毫秒）。
  // timeout_ms <= 0 表示无限期阻塞（使用平台的默认行为）。
  // 成功返回 OK，失败返回错误 Status。
  virtual Status connect(const char* host, int port, int timeout_ms) = 0;

  // 通过套接字发送 |data| 中的 |len| 个字节。
  // 成功时返回实际发送的字节数，失败时返回 Error。
  // 发送字节数不足不一定是错误——调用方应在返回值小于 |len| 时继续循环发送。
  virtual Result<size_t> send(const void* data, size_t len) = 0;

  // 从套接字接收最多 |max_len| 个字节到 |buf|。
  // 成功时返回实际接收的字节数，失败时返回 Error。
  // 返回 0 字节表示对端已正常关闭连接（EOF）。
  virtual Result<size_t> recv(void* buf, size_t max_len) = 0;

  // 关闭套接字。close() 返回后，不得再对该套接字调用 send/recv。
  // 对于正确实现的子类，多次调用 close() 是安全的（幂等）。
  virtual Status close() = 0;

 protected:
  // 默认构造函数，仅允许派生类访问。
  Socket() = default;

 private:
  // 禁止拷贝和移动。
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
};

// ============================================================================
// 平台相关 TCP 套接字前向声明
// ============================================================================
// 这些类定义在各自平台的源代码文件中：
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
// SocketFactory — 平台相关套接字创建的静态工厂
// ============================================================================
class SocketFactory {
 public:
  // 创建一个新的平台相关 TCP 套接字。返回的 Socket* 尚未连接，
  // 调用方需单独调用 connect()。
  //
  // |host| 是远程主机名或 IP 地址（以 null 结尾的字符串）。
  // |port| 是远程 TCP 端口（主机字节序）。
  //
  // 成功时返回指向新 Socket 的指针，失败时返回 Error
  // （例如内存不足、不支持的地址族）。
  static Result<Socket*> create(const char* host, int port);

  // 销毁之前由 SocketFactory::create() 创建的套接字。
  // 内部会在释放内存前自动调用 close()。
  // 传入 nullptr 是安全的（无操作）。
  static void destroy(Socket* s);

 private:
  SocketFactory() = delete;
};

}  // namespace xnet

#endif  // XNET_SOCKET_H_
