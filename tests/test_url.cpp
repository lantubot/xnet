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
//
// ---------------------------------------------------------------------------
// Unit tests for Url parser and percent-encoding (RFC 3986).
// ---------------------------------------------------------------------------

#include "xnet/url.h"
#include "test_helpers.h"

#include <cstring>   // strlen, strncmp, strstr
#include <cstdio>    // printf

using namespace xnet;

// Helpers for comparing StringView fields
// ---------------------------------------------------------------------------

// Returns true if |sv| holds exactly the characters of |expected|.
bool SvEq(const StringView& sv, const char* expected) {
  if (expected == nullptr) return sv.empty();
  size_t len = std::strlen(expected);
  return sv.size() == len && std::strncmp(sv.data(), expected, len) == 0;
}

#define XNET_ASSERT_SV_EQ(sv, expected) \
  XNET_ASSERT(SvEq(sv, expected))

// ============================================================================
// Test 1: Parse simple http:// URL
// ============================================================================
XNET_TEST(ParseSimpleHttp) {
  const char* input = "http://example.com/path/to/page";
  auto result = Url::parse(input, std::strlen(input));
  XNET_ASSERT(result.is_ok());

  const Url& url = result.value();
  XNET_ASSERT_SV_EQ(url.scheme, "http");
  XNET_ASSERT_SV_EQ(url.host, "example.com");
  XNET_ASSERT_SV_EQ(url.path, "/path/to/page");
  XNET_ASSERT(url.port == 0);        // no port specified
  XNET_ASSERT(url.query.empty());
  XNET_ASSERT(url.fragment.empty());
  XNET_ASSERT(url.username.empty());
  XNET_ASSERT(url.password.empty());
}

// ============================================================================
// Test 2: Parse https:// URL with port
// ============================================================================
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

// ============================================================================
// Test 3: Parse URL with query and fragment
// ============================================================================
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

// ============================================================================
// Test 4: Parse URL with user:password
// ============================================================================
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

// ============================================================================
// Test 5: Parse relative path (no scheme)
// ============================================================================
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

// ============================================================================
// Test 6: Parse IPv6 URL (bracketed IPv6 literal with port)
// ============================================================================
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

  // Also test longer IPv6 address
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

// ============================================================================
// Test 7: Parse empty URL (should fail)
// ============================================================================
XNET_TEST(ParseEmptyUrlFails) {
  // Empty string
  {
    auto result = Url::parse("", static_cast<size_t>(0));
    XNET_ASSERT(result.is_err());
  }

  // nullptr with zero length
  {
    auto result = Url::parse(nullptr, static_cast<size_t>(0));
    XNET_ASSERT(result.is_err());
  }
}

// ============================================================================
// Test 8: URL encode / decode roundtrip
// ============================================================================
XNET_TEST(EncodeDecodeRoundtrip) {
  // --- Roundtrip 1: mixed unreserved + reserved characters ----------------
  const char* input1 = "hello world!@#$%^&*()_+-=[]{}|;':\",./<>?`~";
  size_t len1 = std::strlen(input1);

  char encoded[512];
  size_t encoded_len = 0;
  bool ok = Url::encode(StringView(input1, len1),
                        encoded, sizeof(encoded), &encoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(encoded_len > 0);
  XNET_ASSERT(encoded_len < sizeof(encoded));

  char decoded[512];
  size_t decoded_len = 0;
  ok = Url::decode(StringView(encoded, encoded_len),
                   decoded, sizeof(decoded), &decoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(decoded_len == len1);
  XNET_ASSERT(std::strncmp(decoded, input1, len1) == 0);

  // Verify specific percent-encoded sequences (uppercase hex)
  XNET_ASSERT(std::strstr(encoded, "%20") != nullptr);   // space
  XNET_ASSERT(std::strstr(encoded, "%25") != nullptr);   // %
  XNET_ASSERT(std::strstr(encoded, "%40") != nullptr);   // @
  XNET_ASSERT(std::strstr(encoded, "%26") != nullptr);   // &

  // --- Roundtrip 2: only unreserved (no change) --------------------------
  const char* input2 = "abc123-._~";
  size_t len2 = std::strlen(input2);

  ok = Url::encode(StringView(input2, len2),
                   encoded, sizeof(encoded), &encoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(encoded_len == len2);  // no expansion
  XNET_ASSERT(std::strncmp(encoded, input2, len2) == 0);

  ok = Url::decode(StringView(encoded, encoded_len),
                   decoded, sizeof(decoded), &decoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(decoded_len == len2);
  XNET_ASSERT(std::strncmp(decoded, input2, len2) == 0);

  // --- Roundtrip 3: '+' decodes to space (form-urlencoded convention) ----
  const char* plus_encoded = "hello+world";
  size_t plus_len = std::strlen(plus_encoded);
  ok = Url::decode(StringView(plus_encoded, plus_len),
                   decoded, sizeof(decoded), &decoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(decoded_len == 11);
  XNET_ASSERT(std::strncmp(decoded, "hello world", 11) == 0);

  // --- Edge: encode with null output (should fail) -----------------------
  ok = Url::encode(StringView("test"), nullptr, 0, nullptr);
  XNET_ASSERT(!ok);

  // --- Edge: decode with null output (should fail) -----------------------
  ok = Url::decode(StringView("%20"), nullptr, 0, nullptr);
  XNET_ASSERT(!ok);

  // --- Edge: truncated percent sequence (should fail) --------------------
  ok = Url::decode(StringView("%2"), decoded, sizeof(decoded), nullptr);
  XNET_ASSERT(!ok);

  // --- Edge: malformed hex (should fail) ---------------------------------
  ok = Url::decode(StringView("%XY"), decoded, sizeof(decoded), nullptr);
  XNET_ASSERT(!ok);

  // --- Edge: buffer too small for decode ---------------------------------
  ok = Url::decode(StringView("hello%20world"),
                   decoded, 1, nullptr);
  XNET_ASSERT(!ok);
}

// ============================================================================
// Test 9: Default port for http (80) and https (443)
//
// Note: Url::parse() only captures what is present in the URL string; it does
// not infer default ports from the scheme.  These tests verify that explicit
// ports are correctly parsed and that absent ports leave port == 0, which a
// higher-level helper can resolve via the scheme.
// ============================================================================
XNET_TEST(DefaultPorts) {
  // http without explicit port → port == 0
  {
    const char* input = "http://example.com/";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT_SV_EQ(url.scheme, "http");
    XNET_ASSERT(url.port == 0);
  }

  // https without explicit port → port == 0
  {
    const char* input = "https://example.com/";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT_SV_EQ(url.scheme, "https");
    XNET_ASSERT(url.port == 0);
  }

  // http with explicit port 80 → port == 80
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

  // https with explicit port 443 → port == 443
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

  // http on a non-default port
  {
    const char* input = "http://example.com:8080/api/v1";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT(url.port == 8080);
  }

  // https on a non-default port
  {
    const char* input = "https://example.com:9443/admin";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT(url.port == 9443);
  }
}

// ============================================================================
// Entry point
// ============================================================================
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
