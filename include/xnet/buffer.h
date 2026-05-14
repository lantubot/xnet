#ifndef XNET_BUFFER_H_
#define XNET_BUFFER_H_

#include <cstddef>
#include <cstdlib>
#include <cstring>

/** @file xnet/buffer.h
 * @brief Network I/O byte buffer with ownership semantics.
 */

namespace xnet {

/** Abstract interface for memory allocation.
 *
 * Users may subclass Allocator to provide custom memory management
 * (pool, arena, slab, etc.) for Buffer's internal storage.
 */
class Allocator {
 public:
  virtual ~Allocator() = default;
  virtual void* allocate(size_t size) = 0;
  virtual void deallocate(void* ptr, size_t size) = 0;
};

/** Default allocator wrapping std::malloc / std::free. */
class MallocAllocator : public Allocator {
 public:
  void* allocate(size_t size) override { return std::malloc(size); }
  void deallocate(void* ptr, size_t size) override {
    (void)size;
    std::free(ptr);
  }
};

/** Owning byte buffer for network I/O.
 *
 * Buffer manages a contiguous, dynamically-allocated memory region and
 * owns the underlying storage.  It is move-only (copy is prohibited);
 * use clone() to obtain a deep copy.
 *
 * Memory is allocated via the supplied Allocator.  If no allocator is
 * given, a default MallocAllocator (malloc/free) is used.
 */
class Buffer {
 public:
  /** Sentinel value meaning "not found", used by find(). */
  static constexpr size_t npos = static_cast<size_t>(-1);

  // --- 构造函数 / 析构函数 / 赋值运算符
  // ---------------------------------------

  /** Construct an empty buffer using the default malloc/free allocator. */
  Buffer() : Buffer(default_allocator()) {}

  /** Construct an empty buffer with a custom allocator.
   * @param allocator  Pointer to the allocator used for all memory operations.
   */
  explicit Buffer(Allocator* allocator)
      : data_(nullptr), size_(0), capacity_(0), allocator_(allocator) {}

  /** Move constructor.  Transfers ownership from @p other; @p other is
   * left in a valid empty state.
   * @param other  The buffer to move from.
   */
  Buffer(Buffer&& other) noexcept
      : data_(other.data_),
        size_(other.size_),
        capacity_(other.capacity_),
        allocator_(other.allocator_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
  }

  /** Move-assignment operator.  Releases current storage (if any) and
   * transfers ownership from @p other.
   * @param other  The buffer to move from.
   * @return       Reference to this buffer.
   */
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

  /** Destructor.  Deallocates owned storage via the allocator. */
  ~Buffer() {
    if (data_ != nullptr) {
      allocator_->deallocate(data_, capacity_);
    }
  }

  /** @name Copy operations (deleted)
   * Copy is prohibited; use clone() for an explicit deep copy.
   */
  //@{
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;
  //@}

  // -- 访问器 ----------------------------------------------------------------

  /** Returns a const pointer to the internal data. */
  constexpr const char* data() const noexcept { return data_; }
  /** Returns a mutable pointer to the internal data. */
  constexpr char* data() noexcept { return data_; }
  /** Returns a mutable pointer to the internal data (alias for data()). */
  constexpr char* mutable_data() noexcept { return data_; }

  /** Returns the number of bytes currently stored. */
  constexpr size_t size() const noexcept { return size_; }
  /** Returns the current capacity of the buffer. */
  constexpr size_t capacity() const noexcept { return capacity_; }
  /** Returns true if the buffer contains zero bytes. */
  constexpr bool empty() const noexcept { return size_ == 0; }

  /** Clears the buffer (sets size to zero without deallocating). */
  constexpr void clear() noexcept { size_ = 0; }

  // -- 元素访问 -------------------------------------------------------------

  /** Accesses the byte at @p index (mutable).
   * @param index  Zero-based byte index; must be < size().
   */
  constexpr char& operator[](size_t index) noexcept { return data_[index]; }
  /** Accesses the byte at @p index (const).
   * @param index  Zero-based byte index; must be < size().
   */
  constexpr const char& operator[](size_t index) const noexcept {
    return data_[index];
  }

  // -- 修改器 ---------------------------------------------------------------

  /** Appends @p len bytes from @p data to the end of the buffer.
   * Reserves additional capacity if needed.
   */
  void append(const void* data, size_t len);
  /** Appends the contents of @p other to the end of this buffer. */
  void append(const Buffer& other);
  /** Appends a single byte to the end of the buffer. */
  void append(char byte);

  /** Removes the first @p n bytes from the buffer. */
  void pop_front(size_t n);
  /** Resizes the buffer to @p new_size.  New bytes are zero-initialized. */
  void resize(size_t new_size);
  /** Ensures capacity is at least @p new_capacity. */
  void reserve(size_t new_capacity);
  /** Swaps the contents of this buffer with @p other. */
  void swap(Buffer& other) noexcept;

  // -- 查找 -----------------------------------------------------------------

  /** Searches for @p needle (of length @p needle_len) in the buffer.
   * @return  The index of the first occurrence, or npos if not found.
   */
  size_t find(const char* needle, size_t needle_len) const;

  /** Searches for character @p c in the buffer.
   * @return  The index of the first occurrence, or npos if not found.
   */
  size_t find(char c) const;

  // -- 克隆（深拷贝）--------------------------------------------------------

  /** Returns a deep copy of this buffer.
   * The clone uses the same allocator as the original.
   */
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
