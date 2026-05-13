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

#ifndef XNET_HTTP_H_
#define XNET_HTTP_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "xnet/buffer.h"
#include "xnet/error.h"
#include "xnet/str_view.h"

namespace xnet {

// ============================================================================
// Method — HTTP 请求方法
// ============================================================================
enum class Method : uint8_t {
  GET = 0,
  POST,
  PUT,
  DELETE,
  HEAD,
  PATCH,
};

// 返回 |m| 对应的线缆格式字符串（例如 GET → "GET"）。
constexpr inline const char* to_string(Method m) {
  switch (m) {
    case Method::GET:
      return "GET";
    case Method::POST:
      return "POST";
    case Method::PUT:
      return "PUT";
    case Method::DELETE:
      return "DELETE";
    case Method::HEAD:
      return "HEAD";
    case Method::PATCH:
      return "PATCH";
    default:
      return "UNKNOWN";
  }
}

// ============================================================================
// Version — HTTP 协议版本
// ============================================================================
enum class Version : uint8_t {
  HTTP_1_0 = 0,
  HTTP_1_1,
  HTTP_2_0,
};

// 返回 |v| 对应的线缆格式字符串（例如 HTTP_1_1 → "HTTP/1.1"）。
constexpr inline const char* to_string(Version v) {
  switch (v) {
    case Version::HTTP_1_0:
      return "HTTP/1.0";
    case Version::HTTP_1_1:
      return "HTTP/1.1";
    case Version::HTTP_2_0:
      return "HTTP/2.0";
    default:
      return "HTTP/1.1";
  }
}

// ============================================================================
// Header — 单个名称/值对。
//
// |name| 和 |value| 均为 StringView。对于解析后的响应，它们指向传递给
// HttpResponse::parse() 的 |header_storage| Buffer。
// ============================================================================
struct Header {
  StringView name;
  StringView value;
};

// ============================================================================
// HttpRequest — 待发送的 HTTP 请求。
//
// 字段均为公开，便于构造。调用者负责确保 |url| 及所有 Header 的 name/value
// 字符串所指向的内存在使用期间有效。可选的 |body| 指针必须在 serialize()
// 的整个调用期间保持有效。
// ============================================================================
struct HttpRequest {
  Method method;
  StringView url;
  Version version;

  // 可内联存储的最大头部数量。
  static constexpr size_t kMaxHeaders = 64;

  Header headers[kMaxHeaders];
  size_t num_headers;

  // 请求体的非拥有指针。可为 nullptr。
  Buffer* body;

  // 将本请求序列化为 HTTP/1.x 线缆格式写入 |out|：
  //   METHOD /path HTTP/1.1\r\n
  //   Header: value\r\n
  //   \r\n
  //   body...
  Status serialize(Buffer& out) const;
};

// ============================================================================
// HttpResponse — 解析后的 HTTP 响应。
//
// parse() 调用成功后，|headers| 中的 StringView 条目指向传入的
// |header_storage| buffer。响应体由本结构体拥有。
// ============================================================================
struct HttpResponse {
  Version version;
  int status_code;
  StringView reason;

  static constexpr size_t kMaxHeaders = 64;

  Header headers[kMaxHeaders];
  size_t num_headers;

  Buffer body;

  // 从 |data|（|len| 字节）中解析 HTTP 响应消息。
  //
  // |header_storage| 用作每个 Header 条目中 StringView 成员的后备存储。
  // 调用者必须确保 |header_storage| 的生命周期超过本 HttpResponse 对象。
  //
  // 成功时返回解析后的响应，失败时返回描述解析错误的 Error。
  static Result<HttpResponse> parse(const char* data, size_t len,
                                    Buffer& header_storage);
};

}  // namespace xnet

#endif  // XNET_HTTP_H_
