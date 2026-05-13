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

// xnet::StringView 的单元测试。
//
// 测试用例：
//  1. 默认构造（empty, size=0）
//  2. 从 const char* 构造
//  3. 从 (ptr, len) 构造
//  4. 比较（==, !=）
//  5. starts_with
//  6. 查找子串和字符
//  7. substr
//  8. to_int（基本解析）
//  9. default 构造和非空视图的 empty()

#include <climits>  // INT_MAX, INT_MIN
#include <cstddef>  // size_t

#include "test_helpers.h"
#include "xnet/str_view.h"

// =========================================================================
// 1. 默认构造
// =========================================================================

XNET_TEST(DefaultConstruction) {
  xnet::StringView sv;

  XNET_ASSERT(sv.data() == nullptr);
  XNET_ASSERT(sv.size() == 0);
  XNET_ASSERT(sv.length() == 0);
  XNET_ASSERT(sv.empty() == true);
}

// =========================================================================
// 2. 从 const char* 构造
// =========================================================================

XNET_TEST(FromConstCharPtr) {
  const char* hello = "hello";
  xnet::StringView sv(hello);

  XNET_ASSERT(sv.data() == hello);
  XNET_ASSERT(sv.size() == 5);
  XNET_ASSERT(sv.length() == 5);
  XNET_ASSERT(sv.empty() == false);
  XNET_ASSERT(sv == xnet::StringView("hello"));

  // Nullptr 应表现得像默认构造一样。
  xnet::StringView sv_null(nullptr);
  XNET_ASSERT(sv_null.data() == nullptr);
  XNET_ASSERT(sv_null.size() == 0);
  XNET_ASSERT(sv_null.empty() == true);
}

// =========================================================================
// 3. 从 (ptr, len) 构造
// =========================================================================

XNET_TEST(FromPtrLen) {
  const char* text = "hello world";
  xnet::StringView sv(text, 5);

  XNET_ASSERT(sv.data() == text);
  XNET_ASSERT(sv.size() == 5);
  XNET_ASSERT(sv == xnet::StringView("hello"));

  // 零长度视图。
  xnet::StringView empty_sv(text, 0);
  XNET_ASSERT(empty_sv.size() == 0);
  XNET_ASSERT(empty_sv.empty() == true);
}

// =========================================================================
// 4. 比较（==, !=）
// =========================================================================

XNET_TEST(Comparison) {
  constexpr xnet::StringView a("abc");
  constexpr xnet::StringView b("abc");
  constexpr xnet::StringView c("xyz");
  constexpr xnet::StringView d("ab");    // 较短
  constexpr xnet::StringView e("abcd");  // 较长
  xnet::StringView empty;

  // 相等性
  XNET_ASSERT(a == b);
  XNET_ASSERT(b == a);
  XNET_ASSERT(empty == xnet::StringView());  // 两者都是默认构造

  // 不相等性
  XNET_ASSERT(a != c);
  XNET_ASSERT(a != d);
  XNET_ASSERT(a != e);
  XNET_ASSERT(empty != a);

  // 自比较
  XNET_ASSERT(a == a);
}

// =========================================================================
// 5. starts_with
// =========================================================================

XNET_TEST(StartsWith) {
  xnet::StringView sv("hello world");

  // 精确前缀匹配。
  XNET_ASSERT(sv.starts_with("hello") == true);

  // 完整字符串作为前缀。
  XNET_ASSERT(sv.starts_with("hello world") == true);

  // 不匹配的前缀。
  XNET_ASSERT(sv.starts_with("world") == false);

  // 前缀比视图长。
  XNET_ASSERT(sv.starts_with("hello world!!!") == false);

  // 非空视图的空前缀。
  XNET_ASSERT(sv.starts_with("") == true);

  // 空视图的空前缀。
  xnet::StringView empty;
  XNET_ASSERT(empty.starts_with("") == true);

  // 空视图的 nullptr 前缀。
  XNET_ASSERT(empty.starts_with(nullptr) == true);

  // 非空视图的 nullptr 前缀。
  XNET_ASSERT(sv.starts_with(nullptr) == false);
}

// =========================================================================
// 6. 查找子串和字符
// =========================================================================

XNET_TEST(FindSubstring) {
  xnet::StringView sv("hello world, hello");

  // 正常查找子串。
  XNET_ASSERT(sv.find("world") == 6);

  // 在开头查找。
  XNET_ASSERT(sv.find("hello") == 0);

  // 从指定位置开始查找——应跳过第一个匹配。
  XNET_ASSERT(sv.find("hello", 1) == 13);

  // 未找到。
  XNET_ASSERT(sv.find("xyz") == xnet::StringView::npos);

  // 空字符串在开头。
  XNET_ASSERT(sv.find("") == 0);

  // 空字符串越过末尾。
  XNET_ASSERT(sv.find("", sv.size()) == sv.size());
  XNET_ASSERT(sv.find("", sv.size() + 1) == xnet::StringView::npos);

  // nullptr 作为搜索字符串。
  XNET_ASSERT(sv.find(nullptr) == xnet::StringView::npos);
}

XNET_TEST(FindChar) {
  xnet::StringView sv("hello world");

  // 正常查找字符。
  XNET_ASSERT(sv.find('w') == 6);

  // 字符在开头。
  XNET_ASSERT(sv.find('h') == 0);

  // 未找到字符。
  XNET_ASSERT(sv.find('z') == xnet::StringView::npos);

  // 从指定位置开始查找。
  XNET_ASSERT(sv.find('o') == 4);
  XNET_ASSERT(sv.find('o', 5) == 7);

  // 空视图。
  xnet::StringView empty;
  XNET_ASSERT(empty.find('a') == xnet::StringView::npos);
}

// =========================================================================
// 7. substr
// =========================================================================

XNET_TEST(Substr) {
  xnet::StringView sv("hello world");

  // 正常子串。
  xnet::StringView sub = sv.substr(0, 5);
  XNET_ASSERT(sub == xnet::StringView("hello"));

  // 中间子串。
  sub = sv.substr(6, 5);
  XNET_ASSERT(sub == xnet::StringView("world"));

  // 子串到末尾（count = npos）。
  sub = sv.substr(6);
  XNET_ASSERT(sub == xnet::StringView("world"));

  // 偏移量超过末尾 → 空。
  sub = sv.substr(100);
  XNET_ASSERT(sub.empty() == true);
  XNET_ASSERT(sub.size() == 0);

  // 计数大于剩余长度 → 截断。
  sub = sv.substr(6, 100);
  XNET_ASSERT(sub == xnet::StringView("world"));

  // 空视图子串。
  xnet::StringView empty;
  sub = empty.substr(0);
  XNET_ASSERT(sub.empty() == true);
}

// =========================================================================
// 8. to_int（基本解析）
// =========================================================================

XNET_TEST(ToInt) {
  // 基本正数测试。
  XNET_ASSERT(xnet::StringView("0").to_int() == 0);
  XNET_ASSERT(xnet::StringView("123").to_int() == 123);
  XNET_ASSERT(xnet::StringView("999").to_int() == 999);

  // 负数。
  XNET_ASSERT(xnet::StringView("-123").to_int() == -123);

  // 前导 '+' 符号。
  XNET_ASSERT(xnet::StringView("+5").to_int() == 5);

  // 边界：只有符号，没有数字。
  XNET_ASSERT(xnet::StringView("-").to_int() == 0);
  XNET_ASSERT(xnet::StringView("+").to_int() == 0);

  // 边界：空字符串。
  XNET_ASSERT(xnet::StringView().to_int() == 0);
  XNET_ASSERT(xnet::StringView("").to_int() == 0);

  // 格式错误（非数字）。
  XNET_ASSERT(xnet::StringView("abc").to_int() == 0);
  XNET_ASSERT(xnet::StringView("12a34").to_int() == 0);

  // 溢出——应钳制到上限。
  XNET_ASSERT(xnet::StringView("2147483647").to_int() == INT_MAX);
  XNET_ASSERT(xnet::StringView("2147483648").to_int() == INT_MAX);
  XNET_ASSERT(xnet::StringView("9999999999").to_int() == INT_MAX);

  // 下溢——应钳制到下限。
  XNET_ASSERT(xnet::StringView("-2147483648").to_int() == INT_MIN);
  XNET_ASSERT(xnet::StringView("-2147483649").to_int() == INT_MIN);
  XNET_ASSERT(xnet::StringView("-9999999999").to_int() == INT_MIN);
}

// =========================================================================
// 9. 默认构造和非空视图的 empty()
// =========================================================================

XNET_TEST(Empty) {
  // 默认构造。
  xnet::StringView default_sv;
  XNET_ASSERT(default_sv.empty() == true);

  // 从 nullptr 构造。
  xnet::StringView null_sv(nullptr);
  XNET_ASSERT(null_sv.empty() == true);

  // 从 (nullptr, 0) 构造。
  xnet::StringView ptr_len_empty(nullptr, 0);
  XNET_ASSERT(ptr_len_empty.empty() == true);

  // 非空视图。
  xnet::StringView nonempty("hello");
  XNET_ASSERT(nonempty.empty() == false);

  // 从 (ptr, 0) 构造。
  const char* text = "hello";
  xnet::StringView explicit_empty(text, 0);
  XNET_ASSERT(explicit_empty.empty() == true);
}

// =========================================================================
// 主函数
// =========================================================================

int main() {
  XNET_RUN_TEST(DefaultConstruction);
  XNET_RUN_TEST(FromConstCharPtr);
  XNET_RUN_TEST(FromPtrLen);
  XNET_RUN_TEST(Comparison);
  XNET_RUN_TEST(StartsWith);
  XNET_RUN_TEST(FindSubstring);
  XNET_RUN_TEST(FindChar);
  XNET_RUN_TEST(Substr);
  XNET_RUN_TEST(ToInt);
  XNET_RUN_TEST(Empty);

  printf("All StringView tests passed.\n");
  return 0;
}
