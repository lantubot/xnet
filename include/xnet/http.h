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

/** @file xnet/http.h
 * @brief HTTP protocol types — Method, Version, Header, HttpRequest,
 * HttpResponse.
 */

#ifndef XNET_HTTP_H_
#define XNET_HTTP_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "xnet/buffer.h"
#include "xnet/error.h"
#include "xnet/str_view.h"

namespace xnet {

/** @brief HTTP request methods (GET, POST, PUT, DELETE, HEAD, PATCH). */
enum class Method : uint8_t {
  GET = 0,
  POST,
  PUT,
  DELETE,
  HEAD,
  PATCH,
};

/** @brief Returns the wire-format string for the given method.
 * @param m The HTTP method.
 * @return C-string such as "GET", "POST", etc., or "UNKNOWN" for unrecognized
 * values.
 */
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

/** @brief HTTP protocol versions (HTTP/1.0, HTTP/1.1, HTTP/2.0). */
enum class Version : uint8_t {
  HTTP_1_0 = 0,
  HTTP_1_1,
  HTTP_2_0,
};

/** @brief Returns the wire-format string for the given HTTP version.
 * @param v The HTTP version.
 * @return C-string such as "HTTP/1.1".
 */
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

/** @brief A single HTTP header name/value pair.
 * Both @c name and @c value are non-owning StringViews pointing to memory
 * managed by the caller.
 */
struct Header {
  StringView name;  /**< Header field name (e.g. "Content-Type"). */
  StringView value; /**< Header field value (e.g. "text/html"). */
};

/** @brief An HTTP request ready to be serialized to wire format.
 * Fields are public. The caller must ensure that the memory backing @c url
 * and each Header's name/value remains valid for the lifetime of this object.
 */
struct HttpRequest {
  Method method;   /**< HTTP method (GET, POST, etc.). */
  StringView url;  /**< Request target URL (e.g. "/index.html"). */
  Version version; /**< HTTP protocol version. */

  /** Maximum number of headers that can be stored inline. */
  static constexpr size_t kMaxHeaders = 64;

  Header headers[kMaxHeaders]; /**< Array of header name/value pairs. */
  size_t num_headers;          /**< Number of headers currently populated. */

  /** Non-owning pointer to the request body. May be @c nullptr. */
  Buffer* body;

  /** @brief Serialize this request into @p out in HTTP/1.x wire format.
   * Output pattern:
   *   METHOD /path HTTP/1.1\r\n
   *   Header: value\r\n
   *   \r\n
   *   body...
   * @param out The buffer to write the serialized request into.
   * @return Status::OK on success, or an error status.
   */
  Status serialize(Buffer& out) const;
};

/** @brief An HTTP response parsed from raw data.
 * After parse() completes, the StringViews in @c headers point into the
 * @p header_storage buffer passed to parse().
 */
struct HttpResponse {
  Version version;   /**< HTTP protocol version. */
  int status_code;   /**< Numeric status code (e.g. 200, 404). */
  StringView reason; /**< Reason phrase (e.g. "OK", "Not Found"). */

  static constexpr size_t kMaxHeaders = 64; /**< Maximum inline headers. */

  Header headers[kMaxHeaders]; /**< Array of parsed header name/value pairs. */
  size_t num_headers;          /**< Number of headers parsed. */

  Buffer body; /**< Response body bytes. */

  /** @brief Parse an HTTP response message from @p data (@p len bytes).
   * @param data            Raw response bytes.
   * @param len             Number of bytes in @p data.
   * @param header_storage  Backing storage for Header StringViews; must
   *                        outlive this HttpResponse.
   * @return Result containing the parsed HttpResponse on success, or an Error.
   */
  static Result<HttpResponse> parse(const char* data, size_t len,
                                    Buffer& header_storage);
};

}  // namespace xnet

#endif  // XNET_HTTP_H_
