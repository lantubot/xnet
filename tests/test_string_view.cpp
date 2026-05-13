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

// Unit tests for xnet::StringView.
//
// Test cases:
//  1. Default construction (empty, size=0)
//  2. From const char*
//  3. From (ptr, len)
//  4. Comparison (==, !=)
//  5. starts_with
//  6. find substring and char
//  7. substr
//  8. to_int (basic parsing)
//  9. empty() on default and non-empty

#include "test_helpers.h"
#include "xnet/string_view.h"

#include <climits>  // INT_MAX, INT_MIN
#include <cstddef>  // size_t

// =========================================================================
// 1. Default construction
// =========================================================================

XNET_TEST(DefaultConstruction) {
  xnet::StringView sv;

  XNET_ASSERT(sv.data() == nullptr);
  XNET_ASSERT(sv.size() == 0);
  XNET_ASSERT(sv.length() == 0);
  XNET_ASSERT(sv.empty() == true);
}

// =========================================================================
// 2. From const char*
// =========================================================================

XNET_TEST(FromConstCharPtr) {
  const char* hello = "hello";
  xnet::StringView sv(hello);

  XNET_ASSERT(sv.data() == hello);
  XNET_ASSERT(sv.size() == 5);
  XNET_ASSERT(sv.length() == 5);
  XNET_ASSERT(sv.empty() == false);
  XNET_ASSERT(sv == xnet::StringView("hello"));

  // Nullptr should behave like default construction.
  xnet::StringView sv_null(nullptr);
  XNET_ASSERT(sv_null.data() == nullptr);
  XNET_ASSERT(sv_null.size() == 0);
  XNET_ASSERT(sv_null.empty() == true);
}

// =========================================================================
// 3. From (ptr, len)
// =========================================================================

XNET_TEST(FromPtrLen) {
  const char* text = "hello world";
  xnet::StringView sv(text, 5);

  XNET_ASSERT(sv.data() == text);
  XNET_ASSERT(sv.size() == 5);
  XNET_ASSERT(sv == xnet::StringView("hello"));

  // Zero-length view.
  xnet::StringView empty_sv(text, 0);
  XNET_ASSERT(empty_sv.size() == 0);
  XNET_ASSERT(empty_sv.empty() == true);
}

// =========================================================================
// 4. Comparison (==, !=)
// =========================================================================

XNET_TEST(Comparison) {
  constexpr xnet::StringView a("abc");
  constexpr xnet::StringView b("abc");
  constexpr xnet::StringView c("xyz");
  constexpr xnet::StringView d("ab");   // shorter
  constexpr xnet::StringView e("abcd"); // longer
  xnet::StringView empty;

  // Equality
  XNET_ASSERT(a == b);
  XNET_ASSERT(b == a);
  XNET_ASSERT(empty == xnet::StringView());  // both default

  // Inequality
  XNET_ASSERT(a != c);
  XNET_ASSERT(a != d);
  XNET_ASSERT(a != e);
  XNET_ASSERT(empty != a);

  // Self-comparison
  XNET_ASSERT(a == a);
}

// =========================================================================
// 5. starts_with
// =========================================================================

XNET_TEST(StartsWith) {
  xnet::StringView sv("hello world");

  // Exact prefix match.
  XNET_ASSERT(sv.starts_with("hello") == true);

  // Full string as prefix.
  XNET_ASSERT(sv.starts_with("hello world") == true);

  // Non-matching prefix.
  XNET_ASSERT(sv.starts_with("world") == false);

  // Prefix longer than view.
  XNET_ASSERT(sv.starts_with("hello world!!!") == false);

  // Empty prefix on non-empty view.
  XNET_ASSERT(sv.starts_with("") == true);

  // Empty prefix on empty view.
  xnet::StringView empty;
  XNET_ASSERT(empty.starts_with("") == true);

  // Nullptr prefix on empty view.
  XNET_ASSERT(empty.starts_with(nullptr) == true);

  // Nullptr prefix on non-empty view.
  XNET_ASSERT(sv.starts_with(nullptr) == false);
}

// =========================================================================
// 6. find substring and char
// =========================================================================

XNET_TEST(FindSubstring) {
  xnet::StringView sv("hello world, hello");

  // Normal substring find.
  XNET_ASSERT(sv.find("world") == 6);

  // Find at start.
  XNET_ASSERT(sv.find("hello") == 0);

  // Find from position — should skip first match.
  XNET_ASSERT(sv.find("hello", 1) == 13);

  // Not found.
  XNET_ASSERT(sv.find("xyz") == xnet::StringView::npos);

  // Empty needle at start.
  XNET_ASSERT(sv.find("") == 0);

  // Empty needle past end.
  XNET_ASSERT(sv.find("", sv.size()) == sv.size());
  XNET_ASSERT(sv.find("", sv.size() + 1) == xnet::StringView::npos);

  // Nullptr needle.
  XNET_ASSERT(sv.find(nullptr) == xnet::StringView::npos);
}

XNET_TEST(FindChar) {
  xnet::StringView sv("hello world");

  // Normal char find.
  XNET_ASSERT(sv.find('w') == 6);

  // Char at start.
  XNET_ASSERT(sv.find('h') == 0);

  // Char not found.
  XNET_ASSERT(sv.find('z') == xnet::StringView::npos);

  // Find from position.
  XNET_ASSERT(sv.find('o') == 4);
  XNET_ASSERT(sv.find('o', 5) == 7);

  // Empty view.
  xnet::StringView empty;
  XNET_ASSERT(empty.find('a') == xnet::StringView::npos);
}

// =========================================================================
// 7. substr
// =========================================================================

XNET_TEST(Substr) {
  xnet::StringView sv("hello world");

  // Normal substring.
  xnet::StringView sub = sv.substr(0, 5);
  XNET_ASSERT(sub == xnet::StringView("hello"));

  // Middle substring.
  sub = sv.substr(6, 5);
  XNET_ASSERT(sub == xnet::StringView("world"));

  // Substring to end (count = npos).
  sub = sv.substr(6);
  XNET_ASSERT(sub == xnet::StringView("world"));

  // Offset past end → empty.
  sub = sv.substr(100);
  XNET_ASSERT(sub.empty() == true);
  XNET_ASSERT(sub.size() == 0);

  // Count larger than remaining → truncated.
  sub = sv.substr(6, 100);
  XNET_ASSERT(sub == xnet::StringView("world"));

  // Empty view substr.
  xnet::StringView empty;
  sub = empty.substr(0);
  XNET_ASSERT(sub.empty() == true);
}

// =========================================================================
// 8. to_int (basic parsing)
// =========================================================================

XNET_TEST(ToInt) {
  // Basic positive.
  XNET_ASSERT(xnet::StringView("0").to_int() == 0);
  XNET_ASSERT(xnet::StringView("123").to_int() == 123);
  XNET_ASSERT(xnet::StringView("999").to_int() == 999);

  // Negative.
  XNET_ASSERT(xnet::StringView("-123").to_int() == -123);

  // Leading '+' sign.
  XNET_ASSERT(xnet::StringView("+5").to_int() == 5);

  // Edge: only sign, no digits.
  XNET_ASSERT(xnet::StringView("-").to_int() == 0);
  XNET_ASSERT(xnet::StringView("+").to_int() == 0);

  // Edge: empty.
  XNET_ASSERT(xnet::StringView().to_int() == 0);
  XNET_ASSERT(xnet::StringView("").to_int() == 0);

  // Malformed (non-digit).
  XNET_ASSERT(xnet::StringView("abc").to_int() == 0);
  XNET_ASSERT(xnet::StringView("12a34").to_int() == 0);

  // Overflow — should clamp.
  XNET_ASSERT(xnet::StringView("2147483647").to_int() == INT_MAX);
  XNET_ASSERT(xnet::StringView("2147483648").to_int() == INT_MAX);
  XNET_ASSERT(xnet::StringView("9999999999").to_int() == INT_MAX);

  // Underflow — should clamp.
  XNET_ASSERT(xnet::StringView("-2147483648").to_int() == INT_MIN);
  XNET_ASSERT(xnet::StringView("-2147483649").to_int() == INT_MIN);
  XNET_ASSERT(xnet::StringView("-9999999999").to_int() == INT_MIN);
}

// =========================================================================
// 9. empty() on default and non-empty
// =========================================================================

XNET_TEST(Empty) {
  // Default constructed.
  xnet::StringView default_sv;
  XNET_ASSERT(default_sv.empty() == true);

  // From nullptr.
  xnet::StringView null_sv(nullptr);
  XNET_ASSERT(null_sv.empty() == true);

  // From (nullptr, 0).
  xnet::StringView ptr_len_empty(nullptr, 0);
  XNET_ASSERT(ptr_len_empty.empty() == true);

  // Non-empty view.
  xnet::StringView nonempty("hello");
  XNET_ASSERT(nonempty.empty() == false);

  // From (ptr, 0).
  const char* text = "hello";
  xnet::StringView explicit_empty(text, 0);
  XNET_ASSERT(explicit_empty.empty() == true);
}

// =========================================================================
// main
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
