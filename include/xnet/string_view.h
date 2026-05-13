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

#ifndef XNET_STRING_VIEW_H_
#define XNET_STRING_VIEW_H_

// 无 STL 依赖。此头文件仅使用编译器内建函数和独立式 C 头文件 <stddef.h>（提供
// size_t 和 nullptr_t）。

#include <stddef.h>

namespace xnet {

// 轻量级非拥有型连续字符序列引用。
//
// StringView 不分配、不复制、不拥有底层存储。调用方必须保证引用的数据
// 在 StringView 的生命周期内保持有效。
//
// 所有方法在可能的情况下均为 constexpr。不使用异常；通过 operator[] 越界
// 访问是未定义行为（不进行边界检查）。

class StringView {
 public:
  static constexpr size_t npos = static_cast<size_t>(-1);

  // -- 构造函数 -----------------------------------------------------------

  // 默认构造函数：指向 nullptr，长度为零。
  constexpr StringView() noexcept : data_(nullptr), size_(0) {}

  // 从以 null 结尾的字符串构造。如果 s 为 nullptr，则行为等同于默认构造函数。
  constexpr StringView(const char* s) noexcept
      : data_(s), size_(s != nullptr ? StrLen(s) : 0) {}

  // 从指针和显式长度构造。调用方保证 |s| 指向的前 |len| 个字符是有效的。
  constexpr StringView(const char* s, size_t len) noexcept
      : data_(s), size_(len) {}

  // 从 nullptr 字面量构造：等价于默认构造。
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr StringView(std::nullptr_t) noexcept : data_(nullptr), size_(0) {}

  // -- 观察器 -------------------------------------------------------------

  // 返回指向底层字符数据的指针。如果默认构造或用 nullptr 构造，可能为 nullptr。
  constexpr const char* data() const noexcept { return data_; }

  // 返回视图中的字符数。
  constexpr size_t size() const noexcept { return size_; }

  // 返回视图中的字符数（size() 的别名）。
  constexpr size_t length() const noexcept { return size_; }

  // 如果视图长度为零则返回 true。
  constexpr bool empty() const noexcept { return size_ == 0; }

  // 返回索引 |i| 处的字符。如果 |i >= size()| 则行为未定义。不进行边界检查。
  constexpr const char& operator[](size_t i) const noexcept { return data_[i]; }

  // -- 比较 -----------------------------------------------------------------

  friend constexpr bool operator==(StringView lhs, StringView rhs) noexcept {
    if (lhs.size_ != rhs.size_) return false;
    if (lhs.data_ == rhs.data_) return true;
    if (lhs.data_ == nullptr || rhs.data_ == nullptr) return false;
    return Compare(lhs.data_, rhs.data_, lhs.size_) == 0;
  }

  friend constexpr bool operator!=(StringView lhs, StringView rhs) noexcept {
    return !(lhs == rhs);
  }

  // 如果此视图以 null 结尾的字符串 |prefix| 开头则返回 true。
  constexpr bool starts_with(const char* prefix) const noexcept {
    if (prefix == nullptr) return empty();
    size_t plen = StrLen(prefix);
    if (plen > size_) return false;
    return Compare(data_, prefix, plen) == 0;
  }

  // 从位置 |pos| 开始搜索子串 |s| 的首次出现。返回匹配的第一个字符的索引，
  // 如果未找到则返回 npos。
  constexpr size_t find(const char* s, size_t pos = 0) const noexcept {
    if (s == nullptr) return npos;
    size_t slen = StrLen(s);
    if (slen == 0) return pos <= size_ ? pos : npos;
    if (pos + slen > size_) return npos;

    for (size_t i = pos; i + slen <= size_; ++i) {
      if (Compare(data_ + i, s, slen) == 0) {
        return i;
      }
    }
    return npos;
  }

  // 从位置 |pos| 开始搜索字符 |c| 的首次出现。返回索引或 npos（如果未找到）。
  constexpr size_t find(char c, size_t pos = 0) const noexcept {
    if (pos >= size_) return npos;
    for (size_t i = pos; i < size_; ++i) {
      if (data_[i] == c) return i;
    }
    return npos;
  }

  // -- 子串 ---------------------------------------------------------------

  // 返回起始于 |offset|、最多 |count| 个字符的子串视图。如果 |offset| 超过
  // size()，返回空视图。如果 |count| 为 npos 或会超出末尾，结果将被截断。
  constexpr StringView substr(size_t offset,
                              size_t count = npos) const noexcept {
    if (offset >= size_) return StringView();
    size_t remaining = size_ - offset;
    size_t len = (count < remaining) ? count : remaining;
    return StringView(data_ + offset, len);
  }

  // -- 工具方法 -----------------------------------------------------------

  // 将视图解析为十进制整数。不跳过前导空白。接受可选的 '-' 前缀。在输入为空
  // 或格式错误时返回 0（与无异常策略一致——使用 empty() 区分）。
  constexpr int to_int() const noexcept {
    if (empty() || data_ == nullptr) return 0;

    size_t i = 0;
    bool negative = false;

    if (data_[i] == '-') {
      negative = true;
      ++i;
    } else if (data_[i] == '+') {
      ++i;
    }

    if (i >= size_) return 0;  // 只有符号，没有数字

    long long result = 0;
    for (; i < size_; ++i) {
      char ch = data_[i];
      if (ch < '0' || ch > '9') return 0;  // 非数字 → 格式错误
      int digit = static_cast<int>(ch - '0');
      // 检查溢出——溢出时钳制到 INT_MAX/INT_MIN。
      // INT_MAX = 2147483647, INT_MIN = -2147483648
      if (!negative) {
        if (result > 214748364 || (result == 214748364 && digit > 7)) {
          return 2147483647;  // INT_MAX
        }
      } else {
        if (result > 214748364 || (result == 214748364 && digit > 8)) {
          return -2147483648;  // INT_MIN
        }
      }
      result = result * 10 + digit;
    }

    return negative ? static_cast<int>(-result) : static_cast<int>(result);
  }

  // 使用 FNV-1a 计算字符串内容的哈希值。
  constexpr size_t hash() const noexcept {
    // FNV-1a 64 位参数（32 位平台上则为 32 位）。
    // 无论平台宽度如何，我们都使用相同的算法；结果类型始终为 size_t，
    // 因此实际位宽因平台而异。
#if defined(__SIZEOF_SIZE_T__) && __SIZEOF_SIZE_T__ >= 8
    constexpr size_t kOffsetBasis = 14695981039346656037ULL;
    constexpr size_t kPrime = 1099511628211ULL;
#else
    constexpr size_t kOffsetBasis = 2166136261U;
    constexpr size_t kPrime = 16777619U;
#endif

    size_t h = kOffsetBasis;
    for (size_t i = 0; i < size_; ++i) {
      h ^= static_cast<unsigned char>(data_[i]);
      h *= kPrime;
    }
    return h;
  }

 private:
  const char* data_;
  size_t size_;

  // constexpr 填充函数：编译期字符串长度计算。
  static constexpr size_t StrLen(const char* s) {
    size_t len = 0;
    while (s[len] != '\0') ++len;
    return len;
  }

  // constexpr 填充函数：编译期有界字符串比较。
  // 相等返回 0，不相等返回非零值（遵循 memcmp 语义）。
  static constexpr int Compare(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      if (a[i] != b[i]) {
        return static_cast<unsigned char>(a[i]) -
               static_cast<unsigned char>(b[i]);
      }
    }
    return 0;
  }
};

// -- 推导指引 (C++17/20) ---------------------------------------------------
// 这些并非严格必需，但有助于提升易用性：
//   StringView sv("hello");    // 推导为 StringView，而非 const char*

}  // namespace xnet

#endif  // XNET_STRING_VIEW_H_
