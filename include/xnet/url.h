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

#ifndef XNET_URL_H_
#define XNET_URL_H_

#include <stddef.h>

#include "xnet/error.h"
#include "xnet/str_view.h"

namespace xnet {

// Url — 解析后的 URL（RFC 3986）
//
// 所有成员为 StringView 引用原始输入字符串。
// 调用方必须保证输入字符串的生存期超过此 Url 对象。
// 解析过程中不分配堆内存。
//
// 示例：
//   Result<Url> r = Url::parse("https://user:pass@host:443/path?q=1#frag", 38);
//   if (r.is_ok()) { ... r.value().scheme, .host, .port ... }
class Url {
 public:
  // -- URL 组件
  // ----------------------------------------------------------------

  StringView scheme;    // "http"、"https" — 无 scheme 时为空
  StringView host;      // "example.com"、"::1"（IPv6 不带方括号）
  StringView path;      // "/index.html"，不存在时为空
  StringView query;     // "q=hello&n=1"，不存在时为空
  StringView fragment;  // "section-2"，不存在时为空
  StringView username;  // "user"，不存在时为空
  StringView password;  // "pass"，不存在时为空
  int port = 0;         // 0 = "未指定"

  // -- 解析 ------------------------------------------------------------------
  //
  // 将 |str|（长度 |len|）解析为 URL。
  // 成功返回解析后的 Url，格式错误时返回 Error。
  //
  // 接受格式：
  //   scheme://authority[/path][?query][#fragment]
  //   scheme:opaque-part
  //   /path[?query][#fragment]
  //   path[?query][#fragment]
  //   ?query[#fragment]
  //   #fragment
  //
  // IPv6 主机须加方括号：http://[::1]:8080/path
  static Result<Url> parse(const char* str, size_t len);

  // -- 百分号编码 -------------------------------------------------------------
  //
  // 对 input 进行百分号编码写入 output（output_size 字节）。成功返回 true。
  // 非保留字符（A-Z、a-z、0-9、- . _ ~）直接通过；其余变为 %XX。
  static bool encode(const StringView& input, char* output, size_t output_size,
                     size_t* written = nullptr);

  // 百分号解码。成功返回 true。'+' 解码为空格。
  static bool decode(const StringView& input, char* output, size_t output_size,
                     size_t* written = nullptr);

 private:
  // 查找 scheme，返回冒号索引。设置 scheme 子串。未找到时返回 npos。
  static size_t ParseScheme(const char* str, size_t len, StringView& scheme);

  // 查找从 pos 开始的 IPv6 右方括号。返回索引或 npos。
  static size_t FindClosingBracket(const char* str, size_t len, size_t pos);

  // 十六进制字符转整数值。无效返回 -1。
  static constexpr inline int HexValue(char c);
};

// 实现见 url.cc

}  // namespace xnet

#endif  // XNET_URL_H_
