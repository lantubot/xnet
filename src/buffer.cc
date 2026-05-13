#include "xnet/buffer.h"

#include <cstring>

namespace xnet {

void Buffer::append(const void* data, size_t len) {
  if (len == 0) return;
  size_t new_size = size_ + len;
  if (new_size > capacity_) reserve(new_size);
  std::memcpy(data_ + size_, data, len);
  size_ = new_size;
}

void Buffer::append(const Buffer& other) { append(other.data_, other.size_); }
void Buffer::append(char byte) { append(&byte, 1); }

void Buffer::pop_front(size_t n) {
  if (n >= size_) {
    size_ = 0;
    return;
  }
  size_t remaining = size_ - n;
  std::memmove(data_, data_ + n, remaining);
  size_ = remaining;
}

void Buffer::resize(size_t new_size) {
  if (new_size > capacity_) reserve(new_size);
  if (new_size > size_) std::memset(data_ + size_, 0, new_size - size_);
  size_ = new_size;
}

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

size_t Buffer::find(const char* needle, size_t needle_len) const {
  if (needle_len == 0) return 0;
  if (needle_len > size_) return npos;
  for (size_t i = 0; i <= size_ - needle_len; ++i)
    if (std::memcmp(data_ + i, needle, needle_len) == 0) return i;
  return npos;
}

Buffer Buffer::clone() const {
  Buffer result(allocator_);
  result.reserve(size_);
  result.append(data_, size_);
  return result;
}

}  // namespace xnet
