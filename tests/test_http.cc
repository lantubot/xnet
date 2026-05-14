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

/** @file test_http.cc
 * @brief Unit tests for HTTP request serialization and response parsing
 *        (HttpRequest::serialize, HttpResponse::parse).
 */

#include <cstring>

#include "test_helpers.h"
#include "xnet/buffer.h"
#include "xnet/error.h"
#include "xnet/http.h"

using namespace xnet;

/** @brief Serialize a minimal GET request and verify the wire format.
 *
 * Constructs an HttpRequest with method=GET, url="/", version=HTTP_1_1,
 * no headers, and no body.  After serialization the Buffer should contain
 * "GET / HTTP/1.1\r\n\r\n".
 */
XNET_TEST(SerializeGet) {
  HttpRequest req;
  req.method = Method::GET;
  req.url = StringView("/");
  req.version = Version::HTTP_1_1;
  req.num_headers = 0;
  req.body = nullptr;

  Buffer buf;
  Status st = req.serialize(buf);
  XNET_ASSERT(st == Status::OK);

  constexpr const char kExpected[] = "GET / HTTP/1.1\r\n\r\n";
  constexpr size_t kExpectedLen = sizeof(kExpected) - 1;  // 16

  XNET_ASSERT(buf.size() == kExpectedLen);
  XNET_ASSERT(std::memcmp(buf.data(), kExpected, kExpectedLen) == 0);
}

/** @brief Serialize a GET request with two headers and verify the wire format.
 *
 * Headers: `Host: example.com` and `Accept: *` + `/`.
 * Expected output includes the request line, both headers, and the
 * trailing CRLF delimiter.
 */
XNET_TEST(SerializeWithHeaders) {
  HttpRequest req;
  req.method = Method::GET;
  req.url = StringView("/");
  req.version = Version::HTTP_1_1;
  req.body = nullptr;

  req.num_headers = 2;
  req.headers[0].name = StringView("Host");
  req.headers[0].value = StringView("example.com");
  req.headers[1].name = StringView("Accept");
  req.headers[1].value = StringView("*/*");

  Buffer buf;
  Status st = req.serialize(buf);
  XNET_ASSERT(st == Status::OK);

  constexpr const char kExpected[] =
      "GET / HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Accept: */*\r\n"
      "\r\n";
  constexpr size_t kExpectedLen = sizeof(kExpected) - 1;

  XNET_ASSERT(buf.size() == kExpectedLen);
  XNET_ASSERT(std::memcmp(buf.data(), kExpected, kExpectedLen) == 0);
}

/** @brief Parse a minimal HTTP response status line.
 *
 * Input: "HTTP/1.1 200 OK\r\n\r\n"
 * Expected: version=HTTP_1_1, status_code=200, reason="OK".
 */
XNET_TEST(ParseStatusLine) {
  Buffer storage;
  storage.reserve(64);
  constexpr const char kRaw[] = "HTTP/1.1 200 OK\r\n\r\n";
  constexpr size_t kRawLen = sizeof(kRaw) - 1;

  Result<HttpResponse> res = HttpResponse::parse(kRaw, kRawLen, storage);
  XNET_ASSERT(res.is_ok() == true);

  const HttpResponse& resp = res.value();
  XNET_ASSERT(resp.version == Version::HTTP_1_1);
  XNET_ASSERT(resp.status_code == 200);
  XNET_ASSERT_STR_EQ("OK", resp.reason.data());
  XNET_ASSERT(resp.num_headers == 0);
}

/** @brief Parse an HTTP response with two headers.
 *
 * Input response includes Content-Type and Content-Length headers.
 * Verifies header count and header name/value pairs.
 */
XNET_TEST(ParseWithHeaders) {
  Buffer storage;
  storage.reserve(256);
  constexpr const char kRaw[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "Content-Length: 42\r\n"
      "\r\n";
  constexpr size_t kRawLen = sizeof(kRaw) - 1;

  Result<HttpResponse> res = HttpResponse::parse(kRaw, kRawLen, storage);
  XNET_ASSERT(res.is_ok() == true);

  const HttpResponse& resp = res.value();
  XNET_ASSERT(resp.version == Version::HTTP_1_1);
  XNET_ASSERT(resp.status_code == 200);
  XNET_ASSERT(resp.num_headers == 2);

  XNET_ASSERT_STR_EQ("Content-Type", resp.headers[0].name.data());
  XNET_ASSERT_STR_EQ("text/html", resp.headers[0].value.data());

  XNET_ASSERT_STR_EQ("Content-Length", resp.headers[1].name.data());
  XNET_ASSERT_STR_EQ("42", resp.headers[1].value.data());
}

/** @brief Parse an HTTP response that includes a body.
 *
 * After the header delimiter \r\n, the remaining bytes are treated
 * as the response body.  Verifies version, status, headers, and
 * body content.
 */
XNET_TEST(ParseWithBody) {
  Buffer storage;
  storage.reserve(256);
  constexpr const char kRaw[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 13\r\n"
      "\r\n"
      "Hello, World!";
  constexpr size_t kRawLen = sizeof(kRaw) - 1;

  Result<HttpResponse> res = HttpResponse::parse(kRaw, kRawLen, storage);
  XNET_ASSERT(res.is_ok() == true);

  const HttpResponse& resp = res.value();
  XNET_ASSERT(resp.version == Version::HTTP_1_1);
  XNET_ASSERT(resp.status_code == 200);
  XNET_ASSERT(resp.num_headers == 2);

  XNET_ASSERT(resp.body.size() == 13);
  XNET_ASSERT(std::memcmp(resp.body.data(), "Hello, World!", 13) == 0);
}

/** @brief Parsing an empty or null input must return an Error.
 *
 * HttpResponse::parse should reject both nullptr and zero-length
 * data with a non-ok Result.
 */
XNET_TEST(ParseEmptyFail) {
  Buffer storage;
  storage.reserve(64);

  // Null pointer
  {
    Result<HttpResponse> res = HttpResponse::parse(nullptr, 0, storage);
    XNET_ASSERT(res.is_err() == true);
  }

  // Non-null but zero length
  {
    const char dummy[] = "";
    Result<HttpResponse> res = HttpResponse::parse(dummy, 0, storage);
    XNET_ASSERT(res.is_err() == true);
  }
}

/** @brief Parsing a response with an unrecognised HTTP version returns
 * UNSUPPORTED_PROTOCOL.
 *
 * "HTTP/1.2" is not in the Version enum and should fail with
 * Status::UNSUPPORTED_PROTOCOL.
 */
XNET_TEST(ParseInvalidVersion) {
  Buffer storage;
  storage.reserve(64);
  constexpr const char kRaw[] = "HTTP/1.2 200 OK\r\n\r\n";
  constexpr size_t kRawLen = sizeof(kRaw) - 1;

  Result<HttpResponse> res = HttpResponse::parse(kRaw, kRawLen, storage);
  XNET_ASSERT(res.is_err() == true);
  XNET_ASSERT(res.error().code == Status::UNSUPPORTED_PROTOCOL);
}

/** @brief Parse a 404 Not Found response.
 *
 * Verifies that status_code == 404 and the reason phrase is
 * "Not Found".
 */
XNET_TEST(ParseNotFoundStatus) {
  Buffer storage;
  storage.reserve(64);
  constexpr const char kRaw[] = "HTTP/1.1 404 Not Found\r\n\r\n";
  constexpr size_t kRawLen = sizeof(kRaw) - 1;

  Result<HttpResponse> res = HttpResponse::parse(kRaw, kRawLen, storage);
  XNET_ASSERT(res.is_ok() == true);

  const HttpResponse& resp = res.value();
  XNET_ASSERT(resp.version == Version::HTTP_1_1);
  XNET_ASSERT(resp.status_code == 404);
  XNET_ASSERT_STR_EQ("Not Found", resp.reason.data());
}

/** @brief Verify that to_string(Method) returns the correct wire-format
 *         string for every Method enumerator.
 */
XNET_TEST(MethodToString) {
  XNET_ASSERT_STR_EQ("GET", to_string(Method::GET));
  XNET_ASSERT_STR_EQ("POST", to_string(Method::POST));
  XNET_ASSERT_STR_EQ("PUT", to_string(Method::PUT));
  XNET_ASSERT_STR_EQ("DELETE", to_string(Method::DELETE));
  XNET_ASSERT_STR_EQ("HEAD", to_string(Method::HEAD));
  XNET_ASSERT_STR_EQ("PATCH", to_string(Method::PATCH));
}

/** @brief Verify that to_string(Version) returns the correct wire-format
 *         string for every Version enumerator.
 */
XNET_TEST(VersionToString) {
  XNET_ASSERT_STR_EQ("HTTP/1.0", to_string(Version::HTTP_1_0));
  XNET_ASSERT_STR_EQ("HTTP/1.1", to_string(Version::HTTP_1_1));
  XNET_ASSERT_STR_EQ("HTTP/2.0", to_string(Version::HTTP_2_0));
}

/** @brief Test runner entry point.  Executes all HTTP test cases. */
int main() {
  XNET_RUN_TEST(SerializeGet);
  XNET_RUN_TEST(SerializeWithHeaders);
  XNET_RUN_TEST(ParseStatusLine);
  XNET_RUN_TEST(ParseWithHeaders);
  XNET_RUN_TEST(ParseWithBody);
  XNET_RUN_TEST(ParseEmptyFail);
  XNET_RUN_TEST(ParseInvalidVersion);
  XNET_RUN_TEST(ParseNotFoundStatus);
  XNET_RUN_TEST(MethodToString);
  XNET_RUN_TEST(VersionToString);

  printf("All tests completed.\n");
  return 0;
}
