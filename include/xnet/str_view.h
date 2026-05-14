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

/** @file str_view.h
 * @brief Lightweight non-owning string view — STL-free std::string_view
 * replacement.
 *
 * Provides a constexpr-capable StringView class with no STL dependency.
 * Only uses compiler intrinsics and <stddef.h>.
 */

#include <stddef.h>
#include <stdint.h>

namespace xnet {

/** @brief A lightweight non-owning view over a contiguous sequence of
 * characters.
 *
 * StringView is the STL-free equivalent of std::string_view. It does **not**
 * allocate, copy, or own the underlying storage — it simply points to memory
 * managed by the caller. All public methods are constexpr-capable, making
 * StringView usable in both runtime and compile-time contexts.
 *
 * @warning The caller **must** guarantee that the referenced character data
 * remains valid for the entire lifetime of this StringView. Destruction,
 * reallocation, or modification of the backing storage before the StringView
 * is destroyed leads to undefined behavior.
 *
 * @note No STL headers are required. This class only depends on <stddef.h>
 * (for size_t) and <stdint.h> (for uint32_t/uint64_t).
 */
class StringView {
 public:
  /** @brief Special value indicating "not found" or "until the end".
   *
   * Used as the return value of find() when the search fails, and as the
   * default count argument to substr() to mean "remainder of the string".
   */
  static constexpr size_t npos = static_cast<size_t>(-1);

  // -- 构造函数 -----------------------------------------------------------

  /** @brief Construct an empty StringView.
   *
   * Points to nullptr with size 0. All observers on a default-constructed
   * StringView reflect the empty state.
   */
  constexpr StringView() noexcept : data_(nullptr), size_(0) {}

  /** @brief Construct a StringView from a null-terminated C-string.
   * @param s Pointer to a null-terminated character string. If nullptr,
   *          the view is empty (data() == nullptr, size() == 0).
   * @note The length is computed at compile time via StrLen() when possible.
   * @warning The string data must outlive this StringView.
   */
  constexpr StringView(const char* s) noexcept
      : data_(s), size_(s != nullptr ? StrLen(s) : 0) {}

  /** @brief Construct a StringView from a pointer and explicit length.
   * @param s Pointer to the first character. May be nullptr only if len == 0.
   * @param len Number of characters in the view.
   * @note The string is **not** required to be null-terminated. This is the
   *       safe way to create a view over a sub-range of a buffer.
   * @warning The data at [s, s + len) must outlive this StringView.
   */
  constexpr StringView(const char* s, size_t len) noexcept
      : data_(s), size_(len) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  /** @brief Construct an empty StringView from nullptr.
   *
   * Implicit conversion from std::nullptr_t so that `StringView sv = nullptr;`
   * produces an empty view rather than a view over a null pointer.
   */
  constexpr StringView(std::nullptr_t) noexcept : data_(nullptr), size_(0) {}

  // -- 观察器 -------------------------------------------------------------

  /** @brief Returns a pointer to the underlying character data.
   * @return const char* pointer to the first character, or nullptr if empty.
   * @note The returned pointer is **not** guaranteed to be null-terminated.
   *       Use size() to determine the valid range.
   */
  constexpr const char* data() const noexcept { return data_; }

  /** @brief Returns the number of characters in the view.
   * @return The length of the string view in characters.
   */
  constexpr size_t size() const noexcept { return size_; }

  /** @brief Returns the number of characters in the view.
   * @return Same as size(). Provided for compatibility with std::string_view.
   */
  constexpr size_t length() const noexcept { return size_; }

  /** @brief Checks whether the view is empty.
   * @return true if size() == 0, false otherwise.
   */
  constexpr bool empty() const noexcept { return size_ == 0; }

  /** @brief Accesses the character at position i without bounds checking.
   * @param i Index of the character to access. Must be < size().
   * @return const reference to the character at index i.
   * @note Behavior is undefined if i >= size().
   */
  constexpr const char& operator[](size_t i) const noexcept { return data_[i]; }

  // -- 比较 -----------------------------------------------------------------

  /** @brief Compares two StringViews for equality.
   *
   * Two views are equal if they have the same length and the same content.
   * Pointer identity is checked first for efficiency.
   * @param lhs Left-hand side StringView.
   * @param rhs Right-hand side StringView.
   * @return true if the views have identical content, false otherwise.
   */
  friend constexpr bool operator==(StringView lhs, StringView rhs) noexcept {
    if (lhs.size_ != rhs.size_) return false;
    if (lhs.data_ == rhs.data_) return true;
    if (lhs.data_ == nullptr || rhs.data_ == nullptr) return false;
    return Compare(lhs.data_, rhs.data_, lhs.size_) == 0;
  }

  /** @brief Compares two StringViews for inequality.
   * @param lhs Left-hand side StringView.
   * @param rhs Right-hand side StringView.
   * @return true if the views differ in content or length, false otherwise.
   */
  friend constexpr bool operator!=(StringView lhs, StringView rhs) noexcept {
    return !(lhs == rhs);
  }

  /** @brief Checks whether the view starts with the given prefix.
   * @param prefix A null-terminated C-string prefix to test against.
   *               If nullptr, returns empty().
   * @return true if the view begins with the characters in prefix,
   *         false otherwise.
   */
  constexpr bool starts_with(const char* prefix) const noexcept {
    if (prefix == nullptr) return empty();
    size_t plen = StrLen(prefix);
    if (plen > size_) return false;
    return Compare(data_, prefix, plen) == 0;
  }

  /** @brief Searches for the first occurrence of a substring.
   * @param s The null-terminated substring to search for. If nullptr,
   *          returns npos.
   * @param pos The position at which to start the search (default: 0).
   * @return The index of the first character of the match, or npos if
   *         the substring is not found.
   */
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

  /** @brief Searches for the first occurrence of a single character.
   * @param c The character to search for.
   * @param pos The position at which to start the search (default: 0).
   * @return The index of the first occurrence of c at or after pos,
   *         or npos if not found.
   */
  constexpr size_t find(char c, size_t pos = 0) const noexcept {
    if (pos >= size_) return npos;
    for (size_t i = pos; i < size_; ++i) {
      if (data_[i] == c) return i;
    }
    return npos;
  }

  // -- 子串 ---------------------------------------------------------------

  /** @brief Extracts a substring from this view.
   * @param offset Starting position (0-based). If >= size(), an empty
   *               StringView is returned.
   * @param count Maximum number of characters to include (default: npos,
   *               meaning "all remaining characters").
   * @return A StringView over the sub-range [offset, offset + count),
   *         automatically clamped to [offset, size()). Returns an empty
   *         view if offset is out of range.
   */
  constexpr StringView substr(size_t offset,
                              size_t count = npos) const noexcept {
    if (offset >= size_) return StringView();
    size_t remaining = size_ - offset;
    size_t len = (count < remaining) ? count : remaining;
    return StringView(data_ + offset, len);
  }

  // -- 工具方法 -----------------------------------------------------------

  /** @brief Parses the view as a decimal integer.
   *
   * Leading whitespace is **not** skipped — only an optional '+' or '-'
   * sign followed by decimal digits is accepted. If the string contains
   * any non-digit character (after an optional sign), the parse fails.
   *
   * @return The parsed integer value. On format error (empty view,
   *         non-digit characters, or a bare sign with no digits), returns 0.
   *         On overflow, clamps to INT_MAX (2147483647) for positive
   *         values and INT_MIN (-2147483648) for negative values.
   */
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

  /** @brief Computes a hash value for the string content.
   *
   * Uses the FNV-1a (Fowler–Noll–Vo) non-cryptographic hash function.
   * On 64-bit platforms the 64-bit variant is used; on 32-bit platforms
   * the 32-bit variant is selected automatically at compile time.
   *
   * @return size_t hash value suitable for use in hash tables.
   */
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

  /** @brief Computes the length of a null-terminated string at compile time.
   * @param s Pointer to a null-terminated character string.
   * @return The number of characters before the terminating null, excluding
   *         the null itself.
   * @note This is a constexpr replacement for strlen() with no STL dependency.
   */
  static constexpr size_t StrLen(const char* s) {
    size_t len = 0;
    while (s[len] != '\0') ++len;
    return len;
  }

  /** @brief Compares two character sequences up to n characters (memcmp
   * semantics).
   * @param a Pointer to the first sequence.
   * @param b Pointer to the second sequence.
   * @param n Maximum number of characters to compare.
   * @return 0 if the sequences are equal for the first n characters.
   *         Otherwise, returns the difference of the first mismatching
   *         characters cast to unsigned char (negative if a[i] < b[i],
   *         positive if a[i] > b[i]).
   * @note This is a constexpr replacement for memcmp() with no STL dependency.
   */
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
