#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

namespace xnet {

// 极简移动辅助（无 STL 依赖）
template <typename T>
struct RemoveReference {
  using type = T;
};
template <typename T>
struct RemoveReference<T&> {
  using type = T;
};
template <typename T>
struct RemoveReference<T&&> {
  using type = T;
};

template <typename T>
typename RemoveReference<T>::type&& move(T&& t) noexcept {
  return static_cast<typename RemoveReference<T>::type&&>(t);
}

// ============================================================================
// Status — 错误码枚举
// ============================================================================
enum class Status : uint8_t {
  OK = 0,
  UNKNOWN = 1,
  INVALID_ARGUMENT = 2,
  NOT_FOUND = 3,
  TIMEOUT = 4,
  CONNECTION_REFUSED = 5,
  CONNECTION_RESET = 6,
  DNS_FAILURE = 7,
  PROTOCOL_ERROR = 8,
  OUT_OF_MEMORY = 9,
  UNSUPPORTED_PROTOCOL = 10,
  SSL_ERROR = 11,
  IO_ERROR = 12,
};

// ============================================================================
// to_string — Status 的可读名称
// ============================================================================
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

// ============================================================================
// Error — 状态码 + 可选的描述信息
// ============================================================================
struct Error {
  Status code;
  const char* message;  // 可选 —— 可为 nullptr

  explicit Error(Status c, const char* m = nullptr) : code(c), message(m) {}

  const char* what() const { return message ? message : to_string(code); }
};

// ============================================================================
// Result<T> — T（成功）或 Error（失败）的标签联合
// ============================================================================
template <typename T>
class Result {
 public:
  // --- 静态工厂 -------------------------------------------------------------
  static Result ok(const T& val) { return Result(val); }
  static Result ok(T&& val) { return Result(move(val)); }
  static Result err(const Error& e) { return Result(e); }

  // --- 构造函数 -------------------------------------------------------------
  Result(const Result& other) : is_ok_(other.is_ok_) {
    if (is_ok_) {
      construct_value(other.storage_.value_);
    } else {
      construct_error(other.storage_.error_);
    }
  }

  Result(Result&& other) noexcept : is_ok_(other.is_ok_) {
    if (is_ok_) {
      construct_value(move(other.storage_.value_));
    } else {
      construct_error(move(other.storage_.error_));
    }
  }

  ~Result() { destroy(); }

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

  // --- 查询 -----------------------------------------------------------------
  bool is_ok() const { return is_ok_; }
  bool is_err() const { return !is_ok_; }

  // --- 访问器 ---------------------------------------------------------------
  T& value() {
    assert(is_ok_);
    return storage_.value_;
  }

  const T& value() const {
    assert(is_ok_);
    return storage_.value_;
  }

  Error& error() {
    assert(!is_ok_);
    return storage_.error_;
  }

  const Error& error() const {
    assert(!is_ok_);
    return storage_.error_;
  }

 private:
  // --- 标签联合存储 ---------------------------------------------------------
  union Storage {
    T value_;
    Error error_;

    Storage() {}   // 不进行默认初始化
    ~Storage() {}  // 由外部管理生命周期
  } storage_;

  bool is_ok_;

  // --- 私有构造函数（由工厂方法使用）----------------------------------------
  explicit Result(const T& val) : is_ok_(true) { construct_value(val); }

  explicit Result(T&& val) : is_ok_(true) { construct_value(move(val)); }

  explicit Result(const Error& e) : is_ok_(false) { construct_error(e); }

  // --- 辅助函数 -------------------------------------------------------------
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

// ============================================================================
// XNET_UNUSED — 抑制未使用变量的警告
// ============================================================================
#define XNET_UNUSED(x) (void)(x)
