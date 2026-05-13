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

#pragma once

#ifndef XNET_STRING_VIEW_H_
#define XNET_STRING_VIEW_H_

// No STL dependencies. This header uses only compiler built-ins and the
// freestanding C header <stddef.h> for size_t and nullptr_t.

#include <stddef.h>

namespace xnet {

// A lightweight non-owning reference to a contiguous sequence of characters.
//
// StringView does not allocate, copy, or own the underlying storage.  It is
// the caller's responsibility to ensure that the referenced data outlives
// the StringView.
//
// All methods are constexpr-capable where possible.  No exceptions are used;
// out-of-range access via operator[] is undefined behaviour (no bounds check).

class StringView {
 public:
  static constexpr size_t npos = static_cast<size_t>(-1);

  // -- Constructors --------------------------------------------------------

  // Default constructor: points to nullptr with zero length.
  constexpr StringView() noexcept : data_(nullptr), size_(0) {}

  // Construct from a null-terminated string.  If s is nullptr, behaves as
  // the default constructor.
  constexpr StringView(const char* s) noexcept
      : data_(s), size_(s != nullptr ? StrLen(s) : 0) {}

  // Construct from a pointer and an explicit length.  The caller guarantees
  // that the first |len| characters at |s| are valid.
  constexpr StringView(const char* s, size_t len) noexcept
      : data_(s), size_(len) {}

  // Construct from nullptr literal: equivalent to default construction.
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr StringView(std::nullptr_t) noexcept : data_(nullptr), size_(0) {}

  // -- Observers -----------------------------------------------------------

  // Returns a pointer to the underlying character data.  May be nullptr if
  // default-constructed or constructed from nullptr.
  constexpr const char* data() const noexcept { return data_; }

  // Returns the number of characters in the view.
  constexpr size_t size() const noexcept { return size_; }

  // Returns the number of characters in the view (alias for size()).
  constexpr size_t length() const noexcept { return size_; }

  // Returns true if the view has zero length.
  constexpr bool empty() const noexcept { return size_ == 0; }

  // Returns the character at index |i|.  Behaviour is undefined if
  // |i >= size()|.  No bounds checking is performed.
  constexpr const char& operator[](size_t i) const noexcept {
    return data_[i];
  }

  // -- Comparison ----------------------------------------------------------

  friend constexpr bool operator==(StringView lhs, StringView rhs) noexcept {
    if (lhs.size_ != rhs.size_) return false;
    if (lhs.data_ == rhs.data_) return true;
    if (lhs.data_ == nullptr || rhs.data_ == nullptr) return false;
    return Compare(lhs.data_, rhs.data_, lhs.size_) == 0;
  }

  friend constexpr bool operator!=(StringView lhs, StringView rhs) noexcept {
    return !(lhs == rhs);
  }

  // Returns true if this view begins with the null-terminated string |prefix|.
  constexpr bool starts_with(const char* prefix) const noexcept {
    if (prefix == nullptr) return empty();
    size_t plen = StrLen(prefix);
    if (plen > size_) return false;
    return Compare(data_, prefix, plen) == 0;
  }

  // Searches for the first occurrence of the substring |s| starting at
  // position |pos|.  Returns the index of the first character of the match,
  // or npos if not found.
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

  // Searches for the first occurrence of character |c| starting at position
  // |pos|.  Returns the index or npos if not found.
  constexpr size_t find(char c, size_t pos = 0) const noexcept {
    if (pos >= size_) return npos;
    for (size_t i = pos; i < size_; ++i) {
      if (data_[i] == c) return i;
    }
    return npos;
  }

  // -- Substring -----------------------------------------------------------

  // Returns a view of a substring starting at |offset| with at most |count|
  // characters.  If |offset| exceeds size(), an empty view is returned.  If
  // |count| is npos or would extend past the end, the result is truncated.
  constexpr StringView substr(size_t offset, size_t count = npos) const
      noexcept {
    if (offset >= size_) return StringView();
    size_t remaining = size_ - offset;
    size_t len = (count < remaining) ? count : remaining;
    return StringView(data_ + offset, len);
  }

  // -- Utility -------------------------------------------------------------

  // Parses the view as a decimal integer.  Leading whitespace is NOT skipped.
  // An optional leading '-' is accepted.  Returns 0 on empty or malformed
  // input (consistent with no-exception policy — use empty() to distinguish).
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

    if (i >= size_) return 0;  // only sign, no digits

    int result = 0;
    for (; i < size_; ++i) {
      char ch = data_[i];
      if (ch < '0' || ch > '9') return 0;  // non-digit → malformed
      int digit = static_cast<int>(ch - '0');
      // Check for overflow — clamp to INT_MAX/INT_MIN on overflow.
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

    return negative ? -result : result;
  }

  // Computes a hash of the string content using FNV-1a.
  constexpr size_t hash() const noexcept {
    // FNV-1a 64-bit parameters (or 32-bit on 32-bit platforms).
    // We use the same algorithm regardless of platform width; the result
    // type is always size_t so the actual bits differ by platform.
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

  // constexpr polyfill for compile-time string length.
  static constexpr size_t StrLen(const char* s) {
    size_t len = 0;
    while (s[len] != '\0') ++len;
    return len;
  }

  // constexpr polyfill for compile-time bounded string comparison.
  // Returns 0 if equal, nonzero otherwise (mirrors memcmp semantics).
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

// -- Deduction guides (C++17/20) -------------------------------------------
// These are not strictly needed but improve ergonomics:
//   StringView sv("hello");    // deduces StringView, not const char*

}  // namespace xnet

#endif  // XNET_STRING_VIEW_H_
