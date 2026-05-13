#ifndef XNET_BUFFER_H_
#define XNET_BUFFER_H_

#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace xnet {

// ---------------------------------------------------------------------------
// Allocator — 内存分配的抽象接口
// ---------------------------------------------------------------------------
class Allocator {
 public:
  virtual ~Allocator() = default;
  virtual void* allocate(size_t size) = 0;
  virtual void deallocate(void* ptr, size_t size) = 0;
};

// ---------------------------------------------------------------------------
// MallocAllocator — 基于 malloc/free 的默认实现
// ---------------------------------------------------------------------------
class MallocAllocator : public Allocator {
 public:
  void* allocate(size_t size) override { return std::malloc(size); }
  void deallocate(void* ptr, size_t size) override {
    (void)size;
    std::free(ptr);
  }
};

// ---------------------------------------------------------------------------
// Buffer — 用于网络 I/O 的字节缓冲区（拥有所有权）
//
// 使用调用者提供的 Allocator（默认 = malloc/free）。可移动但不可拷贝；
// 需要显式深拷贝时请使用 clone()。
// ---------------------------------------------------------------------------
class Buffer {
 public:
  static constexpr size_t npos = static_cast<size_t>(-1);

  // --- 构造函数 / 析构函数 / 赋值运算符 ---------------------------------------

  Buffer() : Buffer(default_allocator()) {}

  explicit Buffer(Allocator* allocator)
      : data_(nullptr), size_(0), capacity_(0), allocator_(allocator) {}

  Buffer(Buffer&& other) noexcept
      : data_(other.data_),
        size_(other.size_),
        capacity_(other.capacity_),
        allocator_(other.allocator_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
  }

  Buffer& operator=(Buffer&& other) noexcept {
    if (this != &other) {
      if (data_ != nullptr) {
        allocator_->deallocate(data_, capacity_);
      }
      data_ = other.data_;
      size_ = other.size_;
      capacity_ = other.capacity_;
      allocator_ = other.allocator_;
      other.data_ = nullptr;
      other.size_ = 0;
      other.capacity_ = 0;
    }
    return *this;
  }

  ~Buffer() {
    if (data_ != nullptr) {
      allocator_->deallocate(data_, capacity_);
    }
  }

  // 禁止隐式拷贝 —— 请显式使用 clone()
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  // --- 访问器 ---------------------------------------------------------------

  const char* data() const { return data_; }
  char* data() { return data_; }
  char* mutable_data() { return data_; }

  size_t size() const { return size_; }
  size_t capacity() const { return capacity_; }
  bool empty() const { return size_ == 0; }

  void clear() { size_ = 0; }

  // --- 元素访问 -------------------------------------------------------------

  char& operator[](size_t index) { return data_[index]; }
  const char& operator[](size_t index) const { return data_[index]; }

  // --- 修改器 ---------------------------------------------------------------

  void append(const void* data, size_t len);
  void append(const Buffer& other);
  void append(char byte);

  void pop_front(size_t n);
  void resize(size_t new_size);
  void reserve(size_t new_capacity);
  void swap(Buffer& other) noexcept;

  // --- 查找 -----------------------------------------------------------------

  size_t find(const char* needle, size_t needle_len) const;

  // --- 克隆（显式深拷贝） ---------------------------------------------------

  Buffer clone() const;

 private:
  static MallocAllocator* default_allocator() {
    static MallocAllocator alloc;
    return &alloc;
  }

  char* data_;
  size_t size_;
  size_t capacity_;
  Allocator* allocator_;
};

}  // namespace xnet

#endif  // XNET_BUFFER_H_
