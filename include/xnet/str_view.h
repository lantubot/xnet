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

#ifndef XNET_STR_VIEW_H_
#define XNET_STR_VIEW_H_

// 无 STL 依赖。仅使用编译器内建和 <stddef.h>。

#include <stddef.h>
#include <stdint.h>

namespace xnet {

// 轻量级非拥有型连续字符序列引用。
// 不分配、不复制、不拥有底层存储。调用方必须保证引用数据
// 在 StringView 生命周期内有效。所有方法尽可能为 constexpr。

class StringView {
 public:
  static constexpr size_t npos = static_cast<size_t>(-1);

  // -- 构造函数 -----------------------------------------------------------

  constexpr StringView() noexcept : data_(nullptr), size_(0) {}

  constexpr StringView(const char* s) noexcept
      : data_(s), size_(s != nullptr ? StrLen(s) : 0) {}

  constexpr StringView(const char* s, size_t len) noexcept
      : data_(s), size_(len) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr StringView(std::nullptr_t) noexcept : data_(nullptr), size_(0) {}

  // -- 观察器 -------------------------------------------------------------

  constexpr const char* data() const noexcept { return data_; }
  constexpr size_t size() const noexcept { return size_; }
  constexpr size_t length() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }
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

  // 是否以 prefix 开头
  constexpr bool starts_with(const char* prefix) const noexcept {
    if (prefix == nullptr) return empty();
    size_t plen = StrLen(prefix);
    if (plen > size_) return false;
    return Compare(data_, prefix, plen) == 0;
  }

  // 搜索子串 s 首次出现，从 pos 开始。返回索引或 npos
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

  // 从 pos 搜索字符 c，返回索引或 npos
  constexpr size_t find(char c, size_t pos = 0) const noexcept {
    if (pos >= size_) return npos;
    for (size_t i = pos; i < size_; ++i) {
      if (data_[i] == c) return i;
    }
    return npos;
  }

  // -- 子串 ---------------------------------------------------------------

  // 返回子串：起始 offset，最多 count 字符。超界则截断或返回空视图。
  constexpr StringView substr(size_t offset,
                              size_t count = npos) const noexcept {
    if (offset >= size_) return StringView();
    size_t remaining = size_ - offset;
    size_t len = (count < remaining) ? count : remaining;
    return StringView(data_ + offset, len);
  }

  // -- 工具方法 -----------------------------------------------------------

  // 解析为十进制整数。不跳过空白。格式错误时返回 0。
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

    uint32_t result = 0;
    for (; i < size_; ++i) {
      char ch = data_[i];
      if (ch < '0' || ch > '9') return 0;  // 非数字 → 格式错误
      uint32_t digit = static_cast<uint32_t>(ch - '0');
      // 检查溢出——溢出时钳制到 INT_MAX/INT_MIN。
      // INT_MAX = 2147483647, INT_MIN = -2147483648
      if (!negative) {
        if (result > 214748364U || (result == 214748364U && digit > 7U)) {
          return 2147483647;  // INT_MAX
        }
      } else {
        if (result > 214748364U || (result == 214748364U && digit > 8U)) {
          return -2147483648;  // INT_MIN
        }
      }
      result = result * 10U + digit;
    }

    // 无符号运算避免有符号溢出，最后再转换
    return negative ? static_cast<int32_t>(-result)
                    : static_cast<int32_t>(result);
  }

  // 计算 FNV-1a 哈希值
  constexpr size_t hash() const noexcept {
    // FNV-1a 参数，按 64/32 位平台选择
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

  // 编译期字符串长度
  static constexpr size_t StrLen(const char* s) {
    size_t len = 0;
    while (s[len] != '\0') ++len;
    return len;
  }

  // 编译期有界字符串比较（memcmp 语义）
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
// StringView sv("hello") 推导为 StringView，而非 const char*

}  // namespace xnet

#endif  // XNET_STRING_VIEW_H_
