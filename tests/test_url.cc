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
// Url 解析器和百分号编码 (RFC 3986) 的单元测试。
// ---------------------------------------------------------------------------

#include "xnet/url.h"
#include "test_helpers.h"

#include <cstring>   // strlen, strncmp, strstr
#include <cstdio>    // printf

using namespace xnet;

// 比较 StringView 字段的辅助函数
// ---------------------------------------------------------------------------

// 如果 |sv| 与 |expected| 的字符串内容完全相等则返回 true。
bool SvEq(const StringView& sv, const char* expected) {
  if (expected == nullptr) return sv.empty();
  size_t len = std::strlen(expected);
  return sv.size() == len && std::strncmp(sv.data(), expected, len) == 0;
}

#define XNET_ASSERT_SV_EQ(sv, expected) \
  XNET_ASSERT(SvEq(sv, expected))

// ============================================================================
// 测试 1: 解析简单的 http:// URL
// ============================================================================
XNET_TEST(ParseSimpleHttp) {
  const char* input = "http://example.com/path/to/page";
  auto result = Url::parse(input, std::strlen(input));
  XNET_ASSERT(result.is_ok());

  const Url& url = result.value();
  XNET_ASSERT_SV_EQ(url.scheme, "http");
  XNET_ASSERT_SV_EQ(url.host, "example.com");
  XNET_ASSERT_SV_EQ(url.path, "/path/to/page");
  XNET_ASSERT(url.port == 0);        // 未指定端口
  XNET_ASSERT(url.query.empty());
  XNET_ASSERT(url.fragment.empty());
  XNET_ASSERT(url.username.empty());
  XNET_ASSERT(url.password.empty());
}

// ============================================================================
// 测试 2: 解析带端口的 https:// URL
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
// 测试 3: 解析带查询和片段的 URL
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
// 测试 4: 解析带用户名:密码的 URL
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
// 测试 5: 解析相对路径（无 scheme）
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
// 测试 6: 解析 IPv6 URL（带端口的中括号 IPv6 字面量）
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

  // 同时测试更长的 IPv6 地址
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
// 测试 7: 解析空 URL（应失败）
// ============================================================================
XNET_TEST(ParseEmptyUrlFails) {
  // 空字符串
  {
    auto result = Url::parse("", static_cast<size_t>(0));
    XNET_ASSERT(result.is_err());
  }

  // 空指针且长度为零
  {
    auto result = Url::parse(nullptr, static_cast<size_t>(0));
    XNET_ASSERT(result.is_err());
  }
}

// ============================================================================
// 测试 8: URL 编码/解码往返测试
// ============================================================================
XNET_TEST(EncodeDecodeRoundtrip) {
  // --- 往返测试 1: 混合非保留字符和保留字符 ----------------
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

  // 验证特定的百分号编码序列（大写十六进制）
  XNET_ASSERT(std::strstr(encoded, "%20") != nullptr);   // 空格
  XNET_ASSERT(std::strstr(encoded, "%25") != nullptr);   // 百分号
  XNET_ASSERT(std::strstr(encoded, "%40") != nullptr);   // @ 符号
  XNET_ASSERT(std::strstr(encoded, "%26") != nullptr);   // & 符号

  // --- 往返测试 2: 仅非保留字符（无变化）--------------------------
  const char* input2 = "abc123-._~";
  size_t len2 = std::strlen(input2);

  ok = Url::encode(StringView(input2, len2),
                   encoded, sizeof(encoded), &encoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(encoded_len == len2);  // 未扩展
  XNET_ASSERT(std::strncmp(encoded, input2, len2) == 0);

  ok = Url::decode(StringView(encoded, encoded_len),
                   decoded, sizeof(decoded), &decoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(decoded_len == len2);
  XNET_ASSERT(std::strncmp(decoded, input2, len2) == 0);

  // --- 往返测试 3: '+' 解码为空格（form-urlencoded 约定）----
  const char* plus_encoded = "hello+world";
  size_t plus_len = std::strlen(plus_encoded);
  ok = Url::decode(StringView(plus_encoded, plus_len),
                   decoded, sizeof(decoded), &decoded_len);
  XNET_ASSERT(ok);
  XNET_ASSERT(decoded_len == 11);
  XNET_ASSERT(std::strncmp(decoded, "hello world", 11) == 0);

  // --- 边界测试: 编码时输出为 null（应失败）-----------------------
  ok = Url::encode(StringView("test"), nullptr, 0, nullptr);
  XNET_ASSERT(!ok);

  // --- 边界测试: 解码时输出为 null（应失败）-----------------------
  ok = Url::decode(StringView("%20"), nullptr, 0, nullptr);
  XNET_ASSERT(!ok);

  // --- 边界测试: 截断的百分号序列（应失败）--------------------
  ok = Url::decode(StringView("%2"), decoded, sizeof(decoded), nullptr);
  XNET_ASSERT(!ok);

  // --- 边界测试: 格式错误的十六进制（应失败）---------------------------------
  ok = Url::decode(StringView("%XY"), decoded, sizeof(decoded), nullptr);
  XNET_ASSERT(!ok);

  // --- 边界测试: 缓冲区太小，不足以解码---------------------------------
  ok = Url::decode(StringView("hello%20world"),
                   decoded, 1, nullptr);
  XNET_ASSERT(!ok);
}

// ============================================================================
// 测试 9: http (80) 和 https (443) 的默认端口
//
// 注意: Url::parse() 仅捕获 URL 字符串中实际存在的内容；它不会
// 从 scheme 推断默认端口。这些测试验证显式端口被正确解析，
// 且未指定端口时 port == 0，可由更上层的辅助函数通过 scheme 解析。
// ============================================================================
XNET_TEST(DefaultPorts) {
  // http 无显式端口 → port == 0
  {
    const char* input = "http://example.com/";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT_SV_EQ(url.scheme, "http");
    XNET_ASSERT(url.port == 0);
  }

  // https 无显式端口 → port == 0
  {
    const char* input = "https://example.com/";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT_SV_EQ(url.scheme, "https");
    XNET_ASSERT(url.port == 0);
  }

  // http 显式端口 80 → port == 80
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

  // https 显式端口 443 → port == 443
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

  // http 使用非默认端口
  {
    const char* input = "http://example.com:8080/api/v1";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT(url.port == 8080);
  }

  // https 使用非默认端口
  {
    const char* input = "https://example.com:9443/admin";
    auto result = Url::parse(input, std::strlen(input));
    XNET_ASSERT(result.is_ok());
    const Url& url = result.value();
    XNET_ASSERT(url.port == 9443);
  }
}

// ============================================================================
// 入口点
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
