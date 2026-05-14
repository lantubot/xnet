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

/** @file socket.h
 * @brief TCP Socket 平台抽象层。
 *
 * 定义与平台无关的 TCP 套接字接口，包括错误类型、抽象 I/O 类及工厂。
 * 无 STL 依赖。C++20。不包含平台头文件。
 */

#include <stddef.h>  // size_t

#include "xnet/error.h"

namespace xnet {

/** SocketError — 套接字专用错误类型。
 *
 * 继承自 Error，提供语义化的命名工厂方法以构造常见套接字错误。
 */
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

/** Socket — TCP 套接字 I/O 抽象接口。
 *
 * 纯虚基类，定义 TCP 连接、收发、关闭的核心操作。
 * 不可拷贝，通过 SocketFactory 创建，由 destroy() 销毁。
 */
class Socket {
 public:
  virtual ~Socket() = default;

  /** 连接到远程 host:port。
   * @param host 远程主机名（null 结尾字符串）。
   * @param port TCP 端口（主机字节序）。
   * @param timeout_ms 超时毫秒数，<= 0 表示无限阻塞。
   * @return Status 成功或 Error。
   */
  virtual Status connect(const char* host, int port, int timeout_ms) = 0;

  /** 发送 len 字节数据到对端。
   * @param data 待发送数据缓冲区。
   * @param len  期望发送的字节数。
   * @return 实际发送的字节数，或 Error。
   */
  virtual Result<size_t> send(const void* data, size_t len) = 0;

  /** 从对端接收最多 max_len 字节到 buf。
   * @param buf     接收缓冲区。
   * @param max_len 缓冲区容量。
   * @return 实际接收的字节数（0 表示对端已关闭 / EOF），或 Error。
   */
  virtual Result<size_t> recv(void* buf, size_t max_len) = 0;

  /** 关闭套接字连接。
   * 多次调用安全（幂等）。
   * @return Status 成功或 Error。
   */
  virtual Status close() = 0;

 protected:
  Socket() = default;

 private:
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
};

/** 平台相关 TCP 套接字前向声明。
 * 定义在各平台源文件：
 *   Linux:   xnet/linux/tcp_socket.h
 *   Windows: xnet/win/tcp_socket.h
 *   ESP32:   xnet/esp32/tcp_socket.h
 */

#ifdef __linux__
class LinuxTcpSocket;
#elif defined(_WIN32) || defined(_WIN64)
class WinTcpSocket;
#elif defined(ESP_PLATFORM) || defined(ESP32)
class Esp32TcpSocket;
#endif

/** SocketFactory — 平台相关套接字创建的静态工厂。
 *
 * 所有方法均为静态，不可实例化。
 * create() 返回的平台具体子类由编译宏决定。
 */
class SocketFactory {
 public:
  /** 创建平台相关 TCP 套接字（未连接）。
   * @param host 远程主机名（null 结尾字符串）。
   * @param port TCP 端口（主机字节序）。
   * @return 成功返回 Socket*，失败返回 Error。
   */
  static Result<Socket*> create(const char* host, int port);

  /** 销毁由 create() 创建的套接字。
   * 内部自动调用 close()。传入 nullptr 安全（无操作）。
   * @param s 要销毁的 Socket 指针，可为 nullptr。
   */
  static void destroy(Socket* s);

 private:
  SocketFactory() = delete;
};

}  // namespace xnet

#endif  // XNET_SOCKET_H_
