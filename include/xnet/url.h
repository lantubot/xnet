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

#ifndef XNET_URL_H_
#define XNET_URL_H_

#include <stddef.h>

#include "xnet/error.h"
#include "xnet/string_view.h"

namespace xnet {

// ============================================================================
// Url — 解析后的 URL（RFC 3986）
//
// 所有成员都是指向原始输入字符串的 StringView 引用。
// 调用方必须确保输入字符串的生存期超过此 Url 对象。
// 解析过程中不分配堆内存。
//
// 示例：
//   Result<Url> r = Url::parse("https://user:pass@host:443/path?q=1#frag", 38);
//   if (r.is_ok()) { ... 使用 r.value().scheme、.host、.port 等 ... }
// ============================================================================
class Url {
 public:
  // -- URL 组件
  // ----------------------------------------------------------------

  StringView scheme;  // 例如 "http"、"https" — 无 scheme 时为空
  StringView host;    // 例如 "example.com"、"::1"（IPv6 不带方括号）
  StringView path;    // 例如 "/index.html"，不存在时为空
  StringView query;   // 例如 "q=hello&n=1"，不存在时为空
  StringView fragment;  // 例如 "section-2"，不存在时为空
  StringView username;  // 例如 "user"，不存在时为空
  StringView password;  // 例如 "pass"，不存在时为空
  int port = 0;         // 0 表示"未指定"

  // -- 解析 ------------------------------------------------------------------
  //
  // 将 |str|（长度为 |len|）解析为 URL。
  // 成功时返回解析后的 Url，输入格式错误时返回 Error（例如空/空指针字符串）。
  //
  // 接受的格式：
  //   scheme://authority[/path][?query][#fragment]
  //   scheme:opaque-part                          （scheme 后无 "://"）
  //   /path[?query][#fragment]                    （相对路径，无 scheme）
  //   path[?query][#fragment]                     （相对路径，无前导 /）
  //   ?query[#fragment]
  //   #fragment
  //
  // IPv6 主机地址必须加方括号：http://[::1]:8080/path
  static Result<Url> parse(const char* str, size_t len);

  // -- 百分号编码 -------------------------------------------------------------
  //
  // 对 |input| 进行百分号编码，写入 |output|（|output_size| 字节）。
  // 成功返回 true；缓冲区过小返回 false。
  // 若 |written| 非空，则写入实际写入的字节数（不追加 null 终止符）。
  // 非保留字符（A-Z、a-z、0-9、- . _ ~）直接通过；其余字符变为
  // %XX（大写十六进制）。
  static bool encode(const StringView& input, char* output, size_t output_size,
                     size_t* written = nullptr);

  // 对 |input| 进行百分号解码，写入 |output|（|output_size| 字节）。
  // 成功返回 true；十六进制序列格式错误或缓冲区过小时返回 false。
  // '+' 解码为空格（application/x-www-form-urlencoded 约定）。
  // 若 |written| 非空，则写入实际写入的字节数。
  static bool decode(const StringView& input, char* output, size_t output_size,
                     size_t* written = nullptr);

 private:
  // 查找 scheme（如果有），返回其后冒号的索引。
  // 成功时设置 |scheme| 为 scheme 子串。未找到 scheme 时返回 npos（RFC 3986
  // 第 3.1 节）。
  static size_t ParseScheme(const char* str, size_t len, StringView& scheme);

  // 查找从 |pos| 开始的 IPv6 字面量的匹配右方括号。
  // 返回 ']' 的索引，格式错误时返回 npos。
  static size_t FindClosingBracket(const char* str, size_t len, size_t pos);

  // 将十六进制字符 [0-9a-fA-F] 转换为整数值。
  // 输入无效时返回 -1。
  static constexpr inline int HexValue(char c);
};

// 实现已移至 url.cc

}  // namespace xnet

#endif  // XNET_URL_H_
