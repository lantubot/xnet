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

/** @file test_url.cc
 * @brief Unit tests for URL parsing, encoding, and decoding.
 */

#include <cstdio>
#include <cstring>

#include "test_helpers.h"
#include "xnet/url.h"

using namespace xnet;

/** @brief Compares a StringView against an expected null-terminated C string.
 * @param sv        The StringView to compare.
 * @param expected  The expected C string, or nullptr to test for emptiness.
 * @return true if the StringView contents match the expected string, or if
 *         expected is nullptr and the StringView is empty.
 */
bool SvEq(const StringView& sv, const char* expected) {
  if (expected == nullptr) return sv.empty();
  size_t len = std::strlen(expected);
  return sv.size() == len && std::strncmp(sv.data(), expected, len) == 0;
}

/** @brief Asserts that a StringView equals an expected C string.
 * @param sv        The StringView to check.
 * @param expected  The expected C string value.
 */
#define XNET_ASSERT_SV_EQ(sv, expected) XNET_ASSERT(SvEq(sv, expected))

/** @brief Parses a simple HTTP URL with scheme, host, and path. */
XNET_TEST(ParseSimpleHttp) {
  const char* input = "http://example.com/path/to/page";
  auto result = Url::parse(input, std::strlen(input));
  XNET_ASSERT(result.is_ok());
  const Url& url = result.value();
  XNET_ASSERT_SV_EQ(url.scheme, "http");
  XNET_ASSERT_SV_EQ(url.host, "example.com");
  XNET_ASSERT_SV_EQ(url.path, "/path/to/page");
  XNET_ASSERT(url.port == 0);
  XNET_ASSERT(url.query.empty());
  XNET_ASSERT(url.fragment.empty());
  XNET_ASSERT(url.username.empty());
  XNET_ASSERT(url.password.empty());
}

/** @brief Parses an HTTPS URL with a non-default port number. */
XNET_TEST(ParseHttpsWithPort) {
  const char* input = "https://example.com:8443/secure";
  auto result = Url::parse(input, std::strlen(input));
  XNET_ASSERT(result.is_ok());
  const Url& url = result.value();
  XNET_ASSERT_SV_EQ(url.scheme, "https");
  XNET_ASSERT_SV_EQ(url.host, "example.com");
  XNET_ASSERT(url.port == 8443);
  XNET_ASSERT_SV_EQ(url.path, "/secure");
  XNET_ASSERT(url.query.empty());
  XNET_ASSERT(url.fragment.empty());
  XNET_ASSERT(url.username.empty());
  XNET_ASSERT(url.password.empty());
}

/** @brief Parses a URL with a query string and fragment identifier. */
XNET_TEST(ParseQueryAndFragment) {
  const char* input = "http://example.com/search?q=hello&n=1#section-2";
  auto result = Url::parse(input, std::strlen(input));
  XNET_ASSERT(result.is_ok());
  const Url& url = result.value();
  XNET_ASSERT_SV_EQ(url.scheme, "http");
  XNET_ASSERT_SV_EQ(url.host, "example.com");
  XNET_ASSERT_SV_EQ(url.path, "/search");
  XNET_ASSERT_SV_EQ(url.query, "q=hello&n=1");
  XNET_ASSERT_SV_EQ(url.fragment, "section-2");
  XNET_ASSERT(url.port == 0);
  XNET_ASSERT(url.username.empty());
  XNET_ASSERT(url.password.empty());
}

/** @brief Parses an FTP URL with username, password, and port. */
XNET_TEST(ParseUserPassword) {
  const char* input = "ftp://user:secret@ftp.example.com:21/files";
  auto result = Url::parse(input, std::strlen(input));
  XNET_ASSERT(result.is_ok());
  const Url& url = result.value();
  XNET_ASSERT_SV_EQ(url.scheme, "ftp");
  XNET_ASSERT_SV_EQ(url.username, "user");
  XNET_ASSERT_SV_EQ(url.password, "secret");
  XNET_ASSERT_SV_EQ(url.host, "ftp.example.com");
  XNET_ASSERT(url.port == 21);
  XNET_ASSERT_SV_EQ(url.path, "/files");
  XNET_ASSERT(url.query.empty());
  XNET_ASSERT(url.fragment.empty());
}

/** @brief Parses a relative path URL (no scheme or host) with a query string.
 */
XNET_TEST(ParseRelativePath) {
  const char* input = "/relative/path?query=1";
  auto result = Url::parse(input, std::strlen(input));
  XNET_ASSERT(result.is_ok());
  const Url& url = result.value();
  XNET_ASSERT(url.scheme.empty());
  XNET_ASSERT(url.host.empty());
  XNET_ASSERT_SV_EQ(url.path, "/relative/path");
  XNET_ASSERT_SV_EQ(url.query, "query=1");
  XNET_ASSERT(url.fragment.empty());
  XNET_ASSERT(url.port == 0);
  XNET_ASSERT(url.username.empty());
  XNET_ASSERT(url.password.empty());
}

/** @brief Parses IPv6 URLs with brackets and optional port, query, and
 * fragment.
 */
XNET_TEST(ParseIPv6Url) {
  const char* input = "http://[::1]:8080/path";
  auto result = Url::parse(input, std::strlen(input));
  XNET_ASSERT(result.is_ok());
  const Url& url = result.value();
  XNET_ASSERT_SV_EQ(url.scheme, "http");
  XNET_ASSERT_SV_EQ(url.host, "::1");
  XNET_ASSERT(url.port == 8080);
  XNET_ASSERT_SV_EQ(url.path, "/path");
  XNET_ASSERT(url.query.empty());
  XNET_ASSERT(url.fragment.empty());

  {
    const char* input2 = "http://[2001:db8::1]:443/page?q=v#frag";
    auto r2 = Url::parse(input2, std::strlen(input2));
    XNET_ASSERT(r2.is_ok());
    const Url& u2 = r2.value();
    XNET_ASSERT_SV_EQ(u2.scheme, "http");
    XNET_ASSERT_SV_EQ(u2.host, "2001:db8::1");
    XNET_ASSERT(u2.port == 443);
    XNET_ASSERT_SV_EQ(u2.path, "/page");
    XNET_ASSERT_SV_EQ(u2.query, "q=v");
    XNET_ASSERT_SV_EQ(u2.fragment, "frag");
  }
}

/** @brief Verifies that empty and null input strings produce parse errors. */
XNET_TEST(ParseEmptyUrlFails) {
  {
    auto result = Url::parse("", static_cast<size_t>(0));
    XNET_ASSERT(result.is_err());
  }
  {
    auto result = Url::parse(nullptr, static_cast<size_t>(0));
    XNET_ASSERT(result.is_err());
  }
}

/** @brief Tests URL encoding / decoding round-trips, plus edge cases for
 *        unreserved characters, plus-to-space conversion, and null output
 *        buffer handling.
 */
XNET_TEST(EncodeDecodeRoundtrip) {
  const char* input1 = "hello world!@#$%^&*()_+-=[]{}|;':\",./<>?`~";
  size_t len1 = std::strlen(input1);

  char encoded[512];
  size_t encoded_len = 0;
  bool ok = Url::encode(StringView(input1, len1), encoded, sizeof(encoded),
                        &encoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(encoded_len > 0);
  XNET_ASSERT(encoded_len < sizeof(encoded));

  char decoded[512];
  size_t decoded_len = 0;
  ok = Url::decode(StringView(encoded, encoded_len), decoded, sizeof(decoded),
                   &decoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(decoded_len == len1);
  XNET_ASSERT(std::strncmp(decoded, input1, len1) == 0);

  XNET_ASSERT(std::strstr(encoded, "%20") != nullptr);
  XNET_ASSERT(std::strstr(encoded, "%25") != nullptr);
  XNET_ASSERT(std::strstr(encoded, "%40") != nullptr);
  XNET_ASSERT(std::strstr(encoded, "%26") != nullptr);

  const char* input2 = "abc123-._~";
  size_t len2 = std::strlen(input2);
  ok = Url::encode(StringView(input2, len2), encoded, sizeof(encoded),
                   &encoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(encoded_len == len2);
  XNET_ASSERT(std::strncmp(encoded, input2, len2) == 0);

  ok = Url::decode(StringView(encoded, encoded_len), decoded, sizeof(decoded),
                   &decoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(decoded_len == len2);
  XNET_ASSERT(std::strncmp(decoded, input2, len2) == 0);

  const char* plus_encoded = "hello+world";
  size_t plus_len = std::strlen(plus_encoded);
  ok = Url::decode(StringView(plus_encoded, plus_len), decoded, sizeof(decoded),
                   &decoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(decoded_len == 11);
  XNET_ASSERT(std::strncmp(decoded, "hello world", 11) == 0);

  ok = Url::encode(StringView("test"), nullptr, 0, nullptr);
  XNET_ASSERT(!ok);

  ok = Url::decode(StringView("%20"), nullptr, 0, nullptr);
  XNET_ASSERT(!ok);

  ok = Url::decode(StringView("%2"), decoded, sizeof(decoded), nullptr);
  XNET_ASSERT(!ok);

  ok = Url::decode(StringView("%XY"), decoded, sizeof(decoded), nullptr);
  XNET_ASSERT(!ok);

  ok = Url::decode(StringView("hello%20world"), decoded, 1, nullptr);
  XNET_ASSERT(!ok);
}

/** @brief Verifies that default ports (80, 443) are parsed correctly and that
 *        explicit default or non-standard port numbers are preserved.
 */
XNET_TEST(DefaultPorts) {
  {
    const char* input = "http://example.com/";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT_SV_EQ(url.scheme, "http");
    XNET_ASSERT(url.port == 0);
  }
  {
    const char* input = "https://example.com/";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT_SV_EQ(url.scheme, "https");
    XNET_ASSERT(url.port == 0);
  }
  {
    const char* input = "http://example.com:80/";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT_SV_EQ(url.scheme, "http");
    XNET_ASSERT_SV_EQ(url.host, "example.com");
    XNET_ASSERT(url.port == 80);
    XNET_ASSERT_SV_EQ(url.path, "/");
  }
  {
    const char* input = "https://example.com:443/secure";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT_SV_EQ(url.scheme, "https");
    XNET_ASSERT_SV_EQ(url.host, "example.com");
    XNET_ASSERT(url.port == 443);
    XNET_ASSERT_SV_EQ(url.path, "/secure");
  }
  {
    const char* input = "http://example.com:8080/api/v1";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT(url.port == 8080);
  }
  {
    const char* input = "https://example.com:9443/admin";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT(url.port == 9443);
  }
}

/** @brief Test runner entry point. Executes all URL unit tests and prints a
 *        summary message.
 */
int main() {
  XNET_RUN_TEST(ParseSimpleHttp);
  XNET_RUN_TEST(ParseHttpsWithPort);
  XNET_RUN_TEST(ParseQueryAndFragment);
  XNET_RUN_TEST(ParseUserPassword);
  XNET_RUN_TEST(ParseRelativePath);
  XNET_RUN_TEST(ParseIPv6Url);
  XNET_RUN_TEST(ParseEmptyUrlFails);
  XNET_RUN_TEST(EncodeDecodeRoundtrip);
  XNET_RUN_TEST(DefaultPorts);

  printf("All URL tests completed.\n");
  return 0;
}
