#ifndef XNET_BUFFER_H_
#define XNET_BUFFER_H_

#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace xnet {

// ---------------------------------------------------------------------------
// Allocator — abstract interface for memory allocation
// ---------------------------------------------------------------------------
class Allocator {
 public:
  virtual ~Allocator() = default;
  virtual void* allocate(size_t size) = 0;
  virtual void deallocate(void* ptr, size_t size) = 0;
};

// ---------------------------------------------------------------------------
// MallocAllocator — default implementation backed by malloc / free
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
// Buffer — owning byte buffer for network I/O
//
// Uses a caller-supplied Allocator (default = malloc/free).  Movable but
// not copyable; use clone() for explicit deep copies.
// ---------------------------------------------------------------------------
class Buffer {
 public:
  static constexpr size_t npos = static_cast<size_t>(-1);

  // --- Constructors / destructor / assignment --------------------------------

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

  // No hidden copies — use clone() explicitly.
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  // --- Accessors ------------------------------------------------------------

  const char* data() const { return data_; }
  char* data() { return data_; }
  char* mutable_data() { return data_; }

  size_t size() const { return size_; }
  size_t capacity() const { return capacity_; }
  bool empty() const { return size_ == 0; }

  void clear() { size_ = 0; }

  // --- Element access -------------------------------------------------------

  char& operator[](size_t index) { return data_[index]; }
  const char& operator[](size_t index) const { return data_[index]; }

  // --- Modifiers ------------------------------------------------------------

  void append(const void* data, size_t len) {
    if (len == 0) return;
    size_t new_size = size_ + len;
    if (new_size > capacity_) {
      reserve(new_size);
    }
    std::memcpy(data_ + size_, data, len);
    size_ = new_size;
  }

  void append(const Buffer& other) { append(other.data_, other.size_); }

  void append(char byte) { append(&byte, 1); }

  void pop_front(size_t n) {
    if (n >= size_) {
      size_ = 0;
      return;
    }
    size_t remaining = size_ - n;
    std::memmove(data_, data_ + n, remaining);
    size_ = remaining;
  }

  void resize(size_t new_size) {
    if (new_size > capacity_) {
      reserve(new_size);
    }
    if (new_size > size_) {
      std::memset(data_ + size_, 0, new_size - size_);
    }
    size_ = new_size;
  }

  void reserve(size_t new_capacity) {
    if (new_capacity <= capacity_) return;
    void* new_data = allocator_->allocate(new_capacity);
    if (data_ != nullptr) {
      std::memcpy(new_data, data_, size_);
      allocator_->deallocate(data_, capacity_);
    }
    data_ = static_cast<char*>(new_data);
    capacity_ = new_capacity;
  }

  void swap(Buffer& other) noexcept {
    char* tmp_data = data_;
    size_t tmp_size = size_;
    size_t tmp_capacity = capacity_;
    Allocator* tmp_alloc = allocator_;

    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;
    allocator_ = other.allocator_;

    other.data_ = tmp_data;
    other.size_ = tmp_size;
    other.capacity_ = tmp_capacity;
    other.allocator_ = tmp_alloc;
  }

  // --- Search ---------------------------------------------------------------

  size_t find(const char* needle, size_t needle_len) const {
    if (needle_len == 0) return 0;
    if (needle_len > size_) return npos;
    for (size_t i = 0; i <= size_ - needle_len; ++i) {
      if (std::memcmp(data_ + i, needle, needle_len) == 0) {
        return i;
      }
    }
    return npos;
  }

  // --- Clone (explicit deep copy) ------------------------------------------

  Buffer clone() const {
    Buffer result(allocator_);
    result.reserve(size_);
    result.append(data_, size_);
    return result;
  }

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
