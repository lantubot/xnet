#pragma once

/** @file error.h
 * @brief Error handling primitives — Status codes, Error type, and Result<T>
 * tagged union.
 */

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

namespace xnet {

/** @brief Remove reference qualifier from a type.
 *
 * Primary template for non-reference types.
 * Used internally by move() to obtain the underlying type.
 */
template <typename T>
struct RemoveReference {
  using type = T;
};

/** @brief RemoveReference specialization for lvalue references. */
template <typename T>
struct RemoveReference<T&> {
  using type = T;
};

/** @brief RemoveReference specialization for rvalue references. */
template <typename T>
struct RemoveReference<T&&> {
  using type = T;
};

/** @brief Cast an lvalue to an rvalue reference, enabling move semantics.
 *
 * This is a minimal stand-in for std::move, provided because the project
 * avoids the C++ standard library.
 * @param t Reference to the object to move.
 * @return An rvalue reference to the object.
 */
template <typename T>
typename RemoveReference<T>::type&& move(T&& t) noexcept {
  return static_cast<typename RemoveReference<T>::type&&>(t);
}

/** @brief Status codes returned by xnet operations.
 *
 * Every function that can fail returns a Result<T> whose Error
 * carries one of these codes.
 */
enum class Status : uint8_t {
  /** @brief Operation completed successfully. */
  OK = 0,
  /** @brief An unknown or generic error occurred. */
  UNKNOWN = 1,
  /** @brief One of the function arguments was invalid. */
  INVALID_ARGUMENT = 2,
  /** @brief The requested resource was not found. */
  NOT_FOUND = 3,
  /** @brief The operation timed out before completing. */
  TIMEOUT = 4,
  /** @brief The remote peer actively refused the connection. */
  CONNECTION_REFUSED = 5,
  /** @brief An established connection was reset by the peer. */
  CONNECTION_RESET = 6,
  /** @brief DNS resolution failed for the target hostname. */
  DNS_FAILURE = 7,
  /** @brief A protocol-level violation was detected. */
  PROTOCOL_ERROR = 8,
  /** @brief Memory allocation failed. */
  OUT_OF_MEMORY = 9,
  /** @brief The protocol or feature is not supported. */
  UNSUPPORTED_PROTOCOL = 10,
  /** @brief An SSL/TLS error occurred. */
  SSL_ERROR = 11,
  /** @brief An I/O error occurred (read/write/select/etc.). */
  IO_ERROR = 12,
};

/** @brief Return a human-readable string for a Status code.
 *
 * @param s The status code to convert.
 * @return A pointer to a statically-allocated string literal.
 */
constexpr inline const char* to_string(Status s) {
  switch (s) {
    case Status::OK:
      return "OK";
    case Status::UNKNOWN:
      return "UNKNOWN";
    case Status::INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case Status::NOT_FOUND:
      return "NOT_FOUND";
    case Status::TIMEOUT:
      return "TIMEOUT";
    case Status::CONNECTION_REFUSED:
      return "CONNECTION_REFUSED";
    case Status::CONNECTION_RESET:
      return "CONNECTION_RESET";
    case Status::DNS_FAILURE:
      return "DNS_FAILURE";
    case Status::PROTOCOL_ERROR:
      return "PROTOCOL_ERROR";
    case Status::OUT_OF_MEMORY:
      return "OUT_OF_MEMORY";
    case Status::UNSUPPORTED_PROTOCOL:
      return "UNSUPPORTED_PROTOCOL";
    case Status::SSL_ERROR:
      return "SSL_ERROR";
    case Status::IO_ERROR:
      return "IO_ERROR";
    default:
      return "UNKNOWN";
  }
}

/** @brief An error value combining a Status code with an optional
 * human-readable message.
 *
 * @note The `message` pointer may be nullptr; in that case what() falls back
 *       to the string representation of the Status code.
 */
struct Error {
  /** @brief The status code categorising the error. */
  Status code;
  /** @brief Optional descriptive string. May be nullptr. */
  const char* message;  // 可为 nullptr

  /** @brief Construct an Error from a status code and optional message.
   *
   * @param c The status code.
   * @param m Null-terminated string literal, or nullptr (default).
   */
  explicit Error(Status c, const char* m = nullptr) : code(c), message(m) {}

  /** @brief Return a human-readable description of the error.
   *
   * If message is non-null it is returned; otherwise the name of the
   * Status code is returned.
   * @return A pointer to a C-style string.
   */
  constexpr const char* what() const {
    return message ? message : to_string(code);
  }
};

/** @brief Tagged union holding either a value of type T (ok) or an Error (err).
 *
 * Result<T> is the primary error-handling type in xnet.  It behaves as a
 * discriminated union whose discriminant is tracked by an internal bool.
 *
 * **Factory functions**
 *   - Result::ok(val)  — construct an ok result
 *   - Result::err(e)   — construct an err result
 *
 * **Accessors**
 *   - value()  — returns a reference to the stored T; asserts is_ok()
 *   - error()  — returns a reference to the stored Error; asserts is_err()
 *   - is_ok()  — true when the result holds a value
 *   - is_err() — true when the result holds an error
 *
 * **Lifecycle**
 *   Value and error objects are placement-new'd into a raw union and
 *   explicitly destroyed in the destructor.  Copy and move constructors
 *   / assignment operators correctly dispatch to the active member.
 *
 * @tparam T The type of the success value.  Must be copy-constructible
 *           and destructible; move-construction is preferred when available.
 */
template <typename T>
class Result {
 public:
  //  -- 工厂 ----------------------------------------------------------------
  /** @brief Construct an ok Result from a const lvalue reference.
   * @param val The value to store.
   * @return A Result containing val.
   */
  static Result ok(const T& val) { return Result(val); }

  /** @brief Construct an ok Result by moving a value in.
   * @param val The value to move.
   * @return A Result containing the moved-from val.
   */
  static Result ok(T&& val) { return Result(move(val)); }

  /** @brief Construct an err Result from an Error.
   * @param e The error to store.
   * @return A Result containing e.
   */
  static Result err(const Error& e) { return Result(e); }

  //  -- 构造/赋值/析构 -----------------------------------------------------
  /** @brief Copy constructor.  Copies the active member of the union.
   * @param other The result to copy from.
   */
  Result(const Result& other) : is_ok_(other.is_ok_) {
    if (is_ok_) {
      construct_value(other.storage_.value_);
    } else {
      construct_error(other.storage_.error_);
    }
  }

  /** @brief Move constructor.  Moves the active member of the union.
   * @param other The result to move from.  Left in a valid-but-unspecified
   * state.
   */
  Result(Result&& other) noexcept : is_ok_(other.is_ok_) {
    if (is_ok_) {
      construct_value(move(other.storage_.value_));
    } else {
      construct_error(move(other.storage_.error_));
    }
  }

  /** @brief Destructor.  Destroys the active member of the union. */
  ~Result() { destroy(); }

  /** @brief Copy assignment operator.
   * @param other The result to copy from.
   * @return Reference to this.
   */
  Result& operator=(const Result& other) {
    if (this != &other) {
      destroy();
      is_ok_ = other.is_ok_;
      if (is_ok_) {
        construct_value(other.storage_.value_);
      } else {
        construct_error(other.storage_.error_);
      }
    }
    return *this;
  }

  /** @brief Move assignment operator.
   * @param other The result to move from.  Left in a valid-but-unspecified
   * state.
   * @return Reference to this.
   */
  Result& operator=(Result&& other) noexcept {
    if (this != &other) {
      destroy();
      is_ok_ = other.is_ok_;
      if (is_ok_) {
        construct_value(move(other.storage_.value_));
      } else {
        construct_error(move(other.storage_.error_));
      }
    }
    return *this;
  }

  //  -- 查询 ----------------------------------------------------------------
  /** @brief Check whether this Result holds a value (i.e. is ok).
   * @return true if the result is ok, false if it is err.
   */
  constexpr bool is_ok() const noexcept { return is_ok_; }

  /** @brief Check whether this Result holds an error (i.e. is err).
   * @return true if the result is err, false if it is ok.
   */
  constexpr bool is_err() const noexcept { return !is_ok_; }

  //  -- 访问器 --------------------------------------------------------------
  /** @brief Access the stored value (mutable).
   * @pre is_ok() must be true.
   * @return Mutable reference to the contained value.
   */
  T& value() {
    assert(is_ok_);
    return storage_.value_;
  }

  /** @brief Access the stored value (const).
   * @pre is_ok() must be true.
   * @return Const reference to the contained value.
   */
  const T& value() const {
    assert(is_ok_);
    return storage_.value_;
  }

  /** @brief Access the stored error (mutable).
   * @pre is_ok() must be false.
   * @return Mutable reference to the contained Error.
   */
  Error& error() {
    assert(!is_ok_);
    return storage_.error_;
  }

  /** @brief Access the stored error (const).
   * @pre is_ok() must be false.
   * @return Const reference to the contained Error.
   */
  const Error& error() const {
    assert(!is_ok_);
    return storage_.error_;
  }

 private:
  // 标签联合存储
  union Storage {
    T value_;
    Error error_;

    Storage() {}   // 不默认初始化
    ~Storage() {}  // 由外部管理生命周期
  } storage_;

  bool is_ok_;

  // 私有构造函数（由工厂使用）
  explicit Result(const T& val) : is_ok_(true) { construct_value(val); }

  explicit Result(T&& val) : is_ok_(true) { construct_value(move(val)); }

  explicit Result(const Error& e) : is_ok_(false) { construct_error(e); }

  // 辅助函数
  void construct_value(const T& val) {
    ::new (static_cast<void*>(&storage_.value_)) T(val);
  }

  void construct_value(T&& val) {
    ::new (static_cast<void*>(&storage_.value_)) T(move(val));
  }

  void construct_error(const Error& e) {
    ::new (static_cast<void*>(&storage_.error_)) Error(e);
  }

  void construct_error(Error&& e) {
    ::new (static_cast<void*>(&storage_.error_)) Error(move(e));
  }

  void destroy() {
    if (is_ok_) {
      storage_.value_.~T();
    } else {
      storage_.error_.~Error();
    }
  }
};

}  // namespace xnet

/** @brief Suppress unused-variable compiler warnings.
 *
 * Usage: XNET_UNUSED(x);
 */
#define XNET_UNUSED(x) (void)(x)
