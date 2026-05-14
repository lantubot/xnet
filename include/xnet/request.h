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

/** @file include/xnet/request.h
 * @brief HTTP request builder and response types for the XNet library.
 *
 * Provides a fluent builder-pattern interface via the Request class for
 * constructing and executing HTTP requests (GET, POST, PUT, DELETE), and
 * the Response struct for inspecting the results.  This is a low-level,
 * no-STL C++20 embedded HTTP client built on top of XNet's socket and
 * URL parsing primitives.
 */

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

/** @brief Represents the result of an HTTP request.
 *
 * Holds the parsed HTTP status line, response headers, and body.
 * Internal Buffer storage owns the header name/value strings and the body
 * data; the StringView-based header entries reference this storage and
 * remain valid for the lifetime of the Response object.
 */
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

  /** @brief Returns the HTTP status code (e.g. 200, 404, 500). */
  constexpr int status_code() const { return status_code_; }

  /** @brief Returns a pointer to the response body data, or nullptr if empty.
   * @return Pointer to body data. Remains valid for the lifetime of this
   * Response.
   */
  inline const char* body() const {
    return body_.empty() ? nullptr : body_.data();
  }

  /** @brief Returns the size of the response body in bytes. */
  inline size_t body_size() const { return body_.size(); }

  /** @brief Returns the HTTP version of the response. */
  constexpr Version version() const { return version_; }

  /** @brief Looks up a response header by name (case-insensitive).
   * @param name  Null-terminated header name to find.
   * @return The header value string, or nullptr if no matching header exists.
   * @note The returned pointer is valid for the lifetime of this Response
   * object.
   */
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

  /** @brief Returns the number of response headers. */
  constexpr size_t num_headers() const { return num_headers_; }

  /** @brief Returns the header at the given index.
   * @param i  Zero-based index into the header array.
   * @pre @c i < num_headers(), otherwise the behavior is undefined.
   */
  constexpr const Header& header_at(size_t i) const { return headers_[i]; }

 private:
  friend class Request;  // Request 构建 Response

  int status_code_;
  Version version_;
  Header headers_[HttpResponse::kMaxHeaders];
  size_t num_headers_;
  Buffer storage_;  // 头部名称/值 StringView 的后备存储
  Buffer body_;     // 拥有的响应体

  /** @brief Case-insensitive comparison of two equal-length ASCII strings.
   * @param a   First string.
   * @param b   Second string.
   * @param len Length of both strings.
   * @return true if the strings are equal ignoring ASCII case.
   */
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

/** @brief Fluent builder for constructing and executing HTTP requests.
 *
 * Uses the builder pattern with a fluent (method-chaining) interface.
 * Typical usage:
 * @code
 *   Result<Response> r = xnet::Request()
 *       .url("https://api.example.com/data")
 *       .method(Method::POST)
 *       .header("Content-Type", "application/json")
 *       .body("{\"key\":\"value\"}", 15)
 *       .timeout(5000)
 *       .perform();
 * @endcode
 *
 * Each builder method returns @c *this, allowing callers to chain calls.
 * The final @ref perform() call executes the request, parses the response,
 * and returns either a @ref Response or an @ref Error.
 */
class Request {
 public:
  /** @brief Default request timeout in milliseconds. */
  static constexpr int kDefaultTimeoutMs = 30000;

  // --------------------------------------------------------------------------
  // 构造
  // --------------------------------------------------------------------------
  Request()
      : method_(Method::GET), timeout_ms_(kDefaultTimeoutMs), num_headers_(0) {}

  // --------------------------------------------------------------------------
  // 构建器方法（流式接口）
  // --------------------------------------------------------------------------

  /** @brief Sets the request URL.
   * @param url  Null-terminated URL string. The string is copied internally
   *             so it does not need to remain valid after the call.
   * @return Reference to this Request for method chaining.
   */
  Request& url(const char* url) {
    if (url != nullptr) {
      url_.clear();
      url_.append(url, std::strlen(url));
      url_.append('\0');  // 为方便使用添加 null 终止
    }
    return *this;
  }

  /** @brief Sets the HTTP method (GET, POST, PUT, DELETE, etc.).
   * @param m  The HTTP method to use.
   * @return Reference to this Request for method chaining.
   */
  Request& method(Method m) {
    method_ = m;
    return *this;
  }

  /** @brief Adds a request header.
   * @param name   Header name (e.g. \"Content-Type\"). Copied internally.
   * @param value  Header value (e.g. \"application/json\"). Copied internally.
   * @return Reference to this Request for method chaining.
   * @note If either @p name or @p value is nullptr, or if the maximum number
   *       of headers (HttpRequest::kMaxHeaders) has been reached, this call
   *       is a no-op.
   */
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

  /** @brief Sets the request body.
   * @param data  Pointer to body data. The data is copied internally so it
   *              does not need to remain valid after the call.
   * @param len   Number of bytes in @p data.
   * @return Reference to this Request for method chaining.
   */
  Request& body(const char* data, size_t len) {
    body_.clear();
    if (data != nullptr && len > 0) {
      body_.append(data, len);
    }
    return *this;
  }

  /** @brief Sets the request timeout in milliseconds.
   * @param ms  Timeout in milliseconds. A value <= 0 means no timeout.
   * @return Reference to this Request for method chaining.
   */
  Request& timeout(int ms) {
    timeout_ms_ = ms;
    return *this;
  }

  /** @brief Executes the HTTP request.
   *
   * Parses the configured URL, resolves the host, opens a TCP connection,
   * serializes and sends the HTTP request (including headers and body),
   * receives the full response, parses the response, and returns the result.
   *
   * @return A @ref Result containing either a populated @ref Response on
   *         success, or an @ref Error describing the failure (DNS failure,
   *         connection refused, timeout, protocol error, etc.).
   */
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

  /** @brief Checks whether a header with the given name already exists
   *        (case-insensitive ASCII comparison).
   * @param name  Null-terminated header name to look for.
   * @return true if a matching header is found.
   */
  bool HasHeader(const char* name) const;

  /** @brief Case-insensitive ASCII comparison, delegates to
   * Response::CaselessCompare.
   */
  static constexpr bool CaselessCompare(const char* a, const char* b,
                                        size_t len) {
    return Response::CaselessCompare(a, b, len);
  }

  /** @brief Formats an unsigned integer as a decimal string.
   * @param buf      Output buffer to write into.
   * @param buf_size Size of the output buffer.
   * @param val      The unsigned integer value to format.
   * @return Number of characters written (may be less than buf_size if
   *         the buffer is too small).
   */
  static constexpr size_t FormatDecimal(char* buf, size_t buf_size,
                                        uint32_t val) {
    if (buf == nullptr || buf_size == 0) return 0;
    char tmp[32];
    size_t idx = 0;
    if (val == 0) {
      tmp[idx++] = '0';
    } else {
      while (val > 0) {
        tmp[idx++] = static_cast<char>('0' + (val % 10));
        val /= 10;
      }
    }
    size_t written = (idx < buf_size) ? idx : buf_size;
    for (size_t i = 0; i < written; ++i) buf[i] = tmp[idx - 1 - i];
    return written;
  }
};

/** @defgroup free_functions HTTP convenience free functions
 * Convenience wrappers around the Request builder for common HTTP methods.
 * @{
 */

/** @brief Performs an HTTP GET request to the given URL.
 * @param url  Null-terminated URL string.
 * @return Result containing the Response or an Error.
 */
inline Result<Response> get(const char* url) {
  return Request().url(url).method(Method::GET).perform();
}

/** @brief Performs an HTTP POST request with the given body.
 * @param url       Null-terminated URL string.
 * @param body      Pointer to the request body data.
 * @param body_len  Length of the request body in bytes.
 * @return Result containing the Response or an Error.
 */
inline Result<Response> post(const char* url, const char* body,
                             size_t body_len) {
  return Request().url(url).method(Method::POST).body(body, body_len).perform();
}

/** @brief Performs an HTTP PUT request with the given body.
 * @param url       Null-terminated URL string.
 * @param body      Pointer to the request body data.
 * @param body_len  Length of the request body in bytes.
 * @return Result containing the Response or an Error.
 */
inline Result<Response> put(const char* url, const char* body,
                            size_t body_len) {
  return Request().url(url).method(Method::PUT).body(body, body_len).perform();
}

/** @brief Performs an HTTP DELETE request to the given URL.
 * @param url  Null-terminated URL string.
 * @return Result containing the Response or an Error.
 */
inline Result<Response> del(const char* url) {
  return Request().url(url).method(Method::DELETE).perform();
}

/** @} */

}  // namespace xnet

#endif  // XNET_REQUEST_H_
