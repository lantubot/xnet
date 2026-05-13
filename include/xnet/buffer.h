#ifndef XNET_BUFFER_H_
#define XNET_BUFFER_H_

#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace xnet {

// Allocator — 内存分配接口
class Allocator {
 public:
  virtual ~Allocator() = default;
  virtual void* allocate(size_t size) = 0;
  virtual void deallocate(void* ptr, size_t size) = 0;
};

// MallocAllocator — malloc/free 默认实现
class MallocAllocator : public Allocator {
 public:
  void* allocate(size_t size) override { return std::malloc(size); }
  void deallocate(void* ptr, size_t size) override {
    (void)size;
    std::free(ptr);
  }
};

// Buffer — 网络 I/O 字节缓冲区（拥有所有权）
// 使用调用者提供的 Allocator（默认 = malloc/free）。可移动不可拷贝。
class Buffer {
 public:
  static constexpr size_t npos = static_cast<size_t>(-1);

  // --- 构造函数 / 析构函数 / 赋值运算符
  // ---------------------------------------

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

  // -- 访问器 ----------------------------------------------------------------

  constexpr const char* data() const noexcept { return data_; }
  constexpr char* data() noexcept { return data_; }
  constexpr char* mutable_data() noexcept { return data_; }

  constexpr size_t size() const noexcept { return size_; }
  constexpr size_t capacity() const noexcept { return capacity_; }
  constexpr bool empty() const noexcept { return size_ == 0; }

  constexpr void clear() noexcept { size_ = 0; }

  // -- 元素访问 -------------------------------------------------------------

  constexpr char& operator[](size_t index) noexcept { return data_[index]; }
  constexpr const char& operator[](size_t index) const noexcept {
    return data_[index];
  }

  // -- 修改器 ---------------------------------------------------------------

  void append(const void* data, size_t len);
  void append(const Buffer& other);
  void append(char byte);

  void pop_front(size_t n);
  void resize(size_t new_size);
  void reserve(size_t new_capacity);
  void swap(Buffer& other) noexcept;

  // -- 查找 -----------------------------------------------------------------

  size_t find(const char* needle, size_t needle_len) const;

  // -- 克隆（深拷贝）--------------------------------------------------------

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
