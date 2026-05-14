#include "xnet/buffer.h"

#include <cstring>

/** @file src/buffer.cc
 * @brief Implementation of the Buffer class.
 */

namespace xnet {

/** Appends @p len bytes from @p data to the end of the buffer.
 * Reserves additional capacity if needed.
 */
void Buffer::append(const void* data, size_t len) {
  if (len == 0) return;
  size_t new_size = size_ + len;
  if (new_size > capacity_) reserve(new_size);
  std::memcpy(data_ + size_, data, len);
  size_ = new_size;
}

/** Appends the contents of @p other to the end of this buffer. */
void Buffer::append(const Buffer& other) { append(other.data_, other.size_); }

/** Appends a single byte to the end of the buffer. */
void Buffer::append(char byte) { append(&byte, 1); }

/** Removes the first @p n bytes from the buffer by shifting remaining
 * data to the front.
 */
void Buffer::pop_front(size_t n) {
  if (n >= size_) {
    size_ = 0;
    return;
  }
  size_t remaining = size_ - n;
  std::memmove(data_, data_ + n, remaining);
  size_ = remaining;
}

/** Resizes the buffer to @p new_size.  If the new size exceeds the
 * current capacity, the buffer is reallocated.  Newly added bytes
 * are zero-initialized.
 */
void Buffer::resize(size_t new_size) {
  if (new_size > capacity_) reserve(new_size);
  if (new_size > size_) std::memset(data_ + size_, 0, new_size - size_);
  size_ = new_size;
}

/** Ensures the buffer has at least @p new_capacity bytes of storage.
 * Reallocates and copies existing data when growing.
 */
void Buffer::reserve(size_t new_capacity) {
  if (new_capacity <= capacity_) return;
  void* new_data = allocator_->allocate(new_capacity);
  if (data_ != nullptr) {
    std::memcpy(new_data, data_, size_);
    allocator_->deallocate(data_, capacity_);
  }
  data_ = static_cast<char*>(new_data);
  capacity_ = new_capacity;
}

/** Exchanges the contents of this buffer with @p other.
 * @param other  The buffer to swap with.
 */
void Buffer::swap(Buffer& other) noexcept {
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

/** Searches for @p needle (of length @p needle_len) within the buffer.
 * @return  The index of the first occurrence, or npos if not found.
 */
size_t Buffer::find(const char* needle, size_t needle_len) const {
  if (needle_len == 0) return 0;
  if (needle_len > size_) return npos;
  for (size_t i = 0; i <= size_ - needle_len; ++i)
    if (std::memcmp(data_ + i, needle, needle_len) == 0) return i;
  return npos;
}

/** Creates and returns a deep copy of this buffer.
 * The clone allocates its own storage via the same allocator.
 */
Buffer Buffer::clone() const {
  Buffer result(allocator_);
  result.reserve(size_);
  result.append(data_, size_);
  return result;
}

/** Searches for character @p c in the buffer.
 * @return  The index of the first occurrence, or npos if not found.
 */
size_t Buffer::find(char c) const {
  for (size_t i = 0; i < size_; ++i)
    if (data_[i] == c) return i;
  return npos;
}

}  // namespace xnet
