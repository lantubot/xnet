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

#ifndef XNET_REQUEST_H_
#define XNET_REQUEST_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "xnet/buffer.h"
#include "xnet/error.h"
#include "xnet/http.h"
#include "xnet/socket.h"
#include "xnet/str_view.h"
#include "xnet/url.h"

namespace xnet {

// ============================================================================
// Response — 存储一次完成的 HTTP 请求的结果。
//
// 拥有一个 Buffer，它同时作为解析后的头部存储和响应体的后备存储。
// 提供状态码、响应体内容和头部查找的高级访问器。
// ============================================================================
struct Response {
  // --------------------------------------------------------------------------
  // 构造
  // --------------------------------------------------------------------------
  Response() : status_code_(0), version_(Version::HTTP_1_1), num_headers_(0) {
    for (size_t i = 0; i < HttpResponse::kMaxHeaders; ++i) {
      headers_[i].name = StringView();
      headers_[i].value = StringView();
    }
  }

  // --------------------------------------------------------------------------
  // 访问器
  // --------------------------------------------------------------------------

  // 返回 HTTP 状态码（如 200、404、500）。
  constexpr int status_code() const { return status_code_; }

  // 返回响应体数据的指针。如果响应没有 body，则返回 nullptr。
  // 返回的指针在 Response 对象的生命周期内有效。
  inline const char* body() const {
    return body_.empty() ? nullptr : body_.data();
  }

  // 返回响应体的大小（字节数）。
  inline size_t body_size() const { return body_.size(); }

  // 返回响应的 HTTP 版本。
  constexpr Version version() const { return version_; }

  // 按名称查找头部（不区分大小写）。
  // 返回头部值，如果找不到匹配的头部则返回 nullptr。
  // 返回的指针在 Response 对象的生命周期内有效。
  const char* header(const char* name) const {
    if (name == nullptr) return nullptr;

    size_t name_len = std::strlen(name);
    for (size_t i = 0; i < num_headers_; ++i) {
      if (headers_[i].name.size() == name_len &&
          CaselessCompare(headers_[i].name.data(), name, name_len)) {
        // 返回以 null 结尾的值字符串。解析时，头部在 |storage_| 中
        // 以末尾 '\0' 存储，因此 StringView 的数据已经是 null 结尾的。
        return headers_[i].value.data();
      }
    }
    return nullptr;
  }

  // 返回响应中的头部数量。
  constexpr size_t num_headers() const { return num_headers_; }

  // 返回索引 |i| 处的头部。如果 |i| >= num_headers()，则行为未定义。
  constexpr const Header& header_at(size_t i) const { return headers_[i]; }

 private:
  friend class Request;  // Request 构建 Response

  int status_code_;
  Version version_;
  Header headers_[HttpResponse::kMaxHeaders];
  size_t num_headers_;
  Buffer storage_;  // 头部名称/值 StringView 的后备存储
  Buffer body_;     // 拥有的响应体

  // --------------------------------------------------------------------------
  // 对两个等长 ASCII 字符串进行不区分大小写的比较。
  // --------------------------------------------------------------------------
  static constexpr bool CaselessCompare(const char* a, const char* b,
                                        size_t len) {
    for (size_t i = 0; i < len; ++i) {
      char ca = a[i];
      char cb = b[i];
      if (ca == cb) continue;
      if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca + 32);
      if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb + 32);
      if (ca != cb) return false;
    }
    return true;
  }
};

// ============================================================================
// Request — 构建器模式的 HTTP 请求。
//
// 使用示例：
//   Result<Response> r = xnet::Request()
//       .url("https://api.example.com/data")
//       .method(Method::POST)
//       .header("Content-Type", "application/json")
//       .body("{\"key\":\"value\"}", 15)
//       .timeout(5000)
//       .perform();
// ============================================================================
class Request {
 public:
  // 默认超时时间（毫秒）。
  static constexpr int kDefaultTimeoutMs = 30000;

  // --------------------------------------------------------------------------
  // 构造
  // --------------------------------------------------------------------------
  Request()
      : method_(Method::GET), timeout_ms_(kDefaultTimeoutMs), num_headers_(0) {}

  // --------------------------------------------------------------------------
  // 构建器方法（流式接口）
  // --------------------------------------------------------------------------

  // 设置请求 URL。字符串在内部拷贝，因此不需要在构建器之外保持有效。
  Request& url(const char* url) {
    if (url != nullptr) {
      url_.clear();
      url_.append(url, std::strlen(url));
      url_.append('\0');  // 为方便使用添加 null 终止
    }
    return *this;
  }

  // 设置 HTTP 方法。
  Request& method(Method m) {
    method_ = m;
    return *this;
  }

  // 添加一个头部。|name| 和 |value| 均在内部拷贝。
  // 返回 Request 引用以支持链式调用。
  Request& header(const char* name, const char* value) {
    if (name == nullptr || value == nullptr) return *this;
    if (num_headers_ >= HttpRequest::kMaxHeaders) return *this;

    size_t name_len = std::strlen(name);
    size_t val_len = std::strlen(value);

    // 将 name 存入 header_storage_
    size_t name_off = header_storage_.size();
    header_storage_.append(name, name_len);
    header_storage_.append('\0');

    // 将 value 存入 header_storage_
    size_t val_off = header_storage_.size();
    header_storage_.append(value, val_len);
    header_storage_.append('\0');

    headers_[num_headers_].name =
        StringView(header_storage_.data() + name_off, name_len);
    headers_[num_headers_].value =
        StringView(header_storage_.data() + val_off, val_len);
    ++num_headers_;

    return *this;
  }

  // 设置请求体。|data| 在内部拷贝，因此不需要在构建器之外保持有效。
  Request& body(const char* data, size_t len) {
    body_.clear();
    if (data != nullptr && len > 0) {
      body_.append(data, len);
    }
    return *this;
  }

  // 设置超时时间（毫秒）。值 <= 0 表示无超时。
  Request& timeout(int ms) {
    timeout_ms_ = ms;
    return *this;
  }

  // --------------------------------------------------------------------------
  // Perform — 执行 HTTP 请求。
  //
  // 解析 URL，连接到远程服务器，发送 HTTP 请求，
  // 接收响应，并返回 Result<Response>。
  //
  // 失败时返回描述问题的 Error（DNS 解析失败、
  // 连接被拒绝、超时、协议错误等）。
  // --------------------------------------------------------------------------
  Result<Response> perform();

 private:
  // --------------------------------------------------------------------------
  // 成员数据
  // --------------------------------------------------------------------------
  Method method_;
  Buffer url_;   // 以 null 结尾的 URL 字符串
  Buffer host_;  // 以 null 结尾的主机字符串（用于 Host 头部）
  Buffer path_;  // 以 null 结尾的请求路径
  int timeout_ms_;
  Header headers_[HttpRequest::kMaxHeaders];
  size_t num_headers_;
  Buffer header_storage_;  // 构建器头部名称/值字符串的存储
  Buffer body_;            // 请求体数据

  // --------------------------------------------------------------------------
  // 检查具有给定名称的头部是否已存在（不区分大小写）。
  // --------------------------------------------------------------------------
  bool HasHeader(const char* name) const;

  // --------------------------------------------------------------------------
  // 不区分大小写的比较（ASCII）。
  // --------------------------------------------------------------------------
  static constexpr bool CaselessCompare(const char* a, const char* b,
                                        size_t len);

  // --------------------------------------------------------------------------
  // 将无符号整数格式化为十进制字符串写入 |buf|。
  // 返回写入的字符数（不包含 null 终止符）。
  // --------------------------------------------------------------------------
  static constexpr size_t FormatDecimal(char* buf, size_t buf_size,
                                        uint32_t val);
};

// ============================================================================
// 自由函数 — 常用 HTTP 方法的便利封装。
// ============================================================================

// 对 |url| 发起 HTTP GET 请求。
inline Result<Response> get(const char* url) {
  return Request().url(url).method(Method::GET).perform();
}

// 对 |url| 发起带有给定 |body| 的 HTTP POST 请求。
inline Result<Response> post(const char* url, const char* body,
                             size_t body_len) {
  return Request().url(url).method(Method::POST).body(body, body_len).perform();
}

// 对 |url| 发起带有给定 |body| 的 HTTP PUT 请求。
inline Result<Response> put(const char* url, const char* body,
                            size_t body_len) {
  return Request().url(url).method(Method::PUT).body(body, body_len).perform();
}

// 对 |url| 发起 HTTP DELETE 请求。
inline Result<Response> del(const char* url) {
  return Request().url(url).method(Method::DELETE).perform();
}

}  // namespace xnet

#endif  // XNET_REQUEST_H_
