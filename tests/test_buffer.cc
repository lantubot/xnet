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

#include <cstring>
#include <utility>

#include "test_helpers.h"
#include "xnet/buffer.h"

using namespace xnet;

/** @brief Verifies that a default-constructed Buffer has null data, zero size,
 *        zero capacity, and is marked as empty.
 */
XNET_TEST(DefaultConstruction) {
  Buffer buf;
  XNET_ASSERT(buf.data() == nullptr);
  XNET_ASSERT(buf.size() == 0);
  XNET_ASSERT(buf.capacity() == 0);
  XNET_ASSERT(buf.empty() == true);
}

/** @brief Tests appending byte arrays to a Buffer and verifies that the
 *        written data matches the expected content after single and
 *        sequential appends.
 */
XNET_TEST(AppendBytes) {
  Buffer buf;
  const char hello[] = "Hello, Buffer!";
  buf.append(hello, sizeof(hello) - 1);
  XNET_ASSERT(buf.size() == 14);
  XNET_ASSERT(buf.empty() == false);
  XNET_ASSERT(buf.data() != nullptr);
  XNET_ASSERT(std::memcmp(buf.data(), hello, 14) == 0);

  const char more[] = " More data.";
  buf.append(more, 10);
  XNET_ASSERT(buf.size() == 24);
  XNET_ASSERT(std::memcmp(buf.data(), "Hello, Buffer! More data.", 24) == 0);
}

/** @brief Tests appending individual characters to a Buffer and validates
 *        that size increases and stored characters are correct.
 */
XNET_TEST(AppendChar) {
  Buffer buf;
  buf.append('a');
  XNET_ASSERT(buf.size() == 1);
  XNET_ASSERT(buf.data()[0] == 'a');
  buf.append('b');
  buf.append('c');
  XNET_ASSERT(buf.size() == 3);
  XNET_ASSERT(buf.data()[0] == 'a');
  XNET_ASSERT(buf.data()[1] == 'b');
  XNET_ASSERT(buf.data()[2] == 'c');
}

/** @brief Tests removing elements from the front of a Buffer via pop_front(),
 *        including zero removal, partial removal, full drain, and
 *        over-removal (beyond current size).
 */
XNET_TEST(PopFront) {
  Buffer buf;
  const char data[] = "ABCDEFGHIJ";
  buf.append(data, 10);

  buf.pop_front(0);
  XNET_ASSERT(buf.size() == 10);

  buf.pop_front(3);
  XNET_ASSERT(buf.size() == 7);
  XNET_ASSERT(std::memcmp(buf.data(), "DEFGHIJ", 7) == 0);

  buf.pop_front(7);
  XNET_ASSERT(buf.size() == 0);
  XNET_ASSERT(buf.empty() == true);

  buf.pop_front(100);
  XNET_ASSERT(buf.size() == 0);
}

/** @brief Tests buffer capacity reservation via reserve(). Verifies that
 *        capacity grows monotonically, that shrinking requests are
 *        ignored, and that existing data survives reallocation.
 */
XNET_TEST(ReserveAndCapacity) {
  Buffer buf;
  XNET_ASSERT(buf.capacity() == 0);

  buf.reserve(64);
  XNET_ASSERT(buf.capacity() >= 64);
  XNET_ASSERT(buf.data() != nullptr);
  XNET_ASSERT(buf.size() == 0);

  buf.reserve(32);
  XNET_ASSERT(buf.capacity() >= 64);

  buf.reserve(128);
  XNET_ASSERT(buf.capacity() >= 128);

  buf.append("hello", 5);
  XNET_ASSERT(buf.size() == 5);
  XNET_ASSERT(std::memcmp(buf.data(), "hello", 5) == 0);
}

/** @brief Tests resizing a Buffer to larger and smaller sizes. Verifies that
 *        growth zero-initializes new elements and that shrinking
 *        correctly truncates and discards the tail.
 */
XNET_TEST(Resize) {
  Buffer buf;

  buf.resize(10);
  XNET_ASSERT(buf.size() == 10);
  XNET_ASSERT(buf.capacity() >= 10);
  for (size_t i = 0; i < 10; ++i) XNET_ASSERT(buf.data()[i] == 0);

  std::memcpy(buf.data(), "ABCDE", 5);

  buf.resize(3);
  XNET_ASSERT(buf.size() == 3);
  XNET_ASSERT(std::memcmp(buf.data(), "ABC", 3) == 0);

  buf.resize(8);
  XNET_ASSERT(buf.size() == 8);
  XNET_ASSERT(std::memcmp(buf.data(), "ABC", 3) == 0);
  XNET_ASSERT(buf.data()[3] == 0);
  XNET_ASSERT(buf.data()[7] == 0);
}

/** @brief Tests swapping the contents of two Buffers via swap(), including
 *        swapping with an empty (default-constructed) Buffer.
 */
XNET_TEST(Swap) {
  Buffer buf_a;
  Buffer buf_b;
  const char data_a[] = "AAAA";
  const char data_b[] = "BBBBBBB";
  buf_a.append(data_a, 4);
  buf_b.append(data_b, 7);

  buf_a.swap(buf_b);
  XNET_ASSERT(buf_a.size() == 7);
  XNET_ASSERT(std::memcmp(buf_a.data(), "BBBBBBB", 7) == 0);
  XNET_ASSERT(buf_b.size() == 4);
  XNET_ASSERT(std::memcmp(buf_b.data(), "AAAA", 4) == 0);

  Buffer buf_c;
  buf_a.swap(buf_c);
  XNET_ASSERT(buf_a.size() == 0);
  XNET_ASSERT(buf_a.data() == nullptr);
  XNET_ASSERT(buf_c.size() == 7);
  XNET_ASSERT(std::memcmp(buf_c.data(), "BBBBBBB", 7) == 0);
}

/** @brief Tests substring search within a Buffer via find(). Covers
 *        matches at the beginning, middle, and end of the data, a
 *        non-existent substring, an empty query, and a query that
 *        exceeds the buffer length.
 */
XNET_TEST(FindSubstring) {
  Buffer buf;
  const char data[] = "The quick brown fox jumps over the lazy dog.";
  buf.append(data, std::strlen(data));

  XNET_ASSERT(buf.find("The", 3) == 0);
  XNET_ASSERT(buf.find("fox", 3) == 16);
  XNET_ASSERT(buf.find("dog", 3) == 40);
  XNET_ASSERT(buf.find("cat", 3) == Buffer::npos);
  XNET_ASSERT(buf.find("", 0) == 0);
  XNET_ASSERT(buf.find("The quick brown fox jumps over the lazy dog. Extra",
                       49) == Buffer::npos);
  XNET_ASSERT(buf.find("q", 1) == 4);
}

/** @brief Tests that clear() resets the Buffer to an empty state (size == 0,
 *        empty == true) while allowing subsequent append operations to
 *        work correctly.
 */
XNET_TEST(ClearResetsToEmpty) {
  Buffer buf;
  buf.append("Hello, world!", 13);
  XNET_ASSERT(buf.size() == 13);
  XNET_ASSERT(buf.empty() == false);

  buf.clear();
  XNET_ASSERT(buf.size() == 0);
  XNET_ASSERT(buf.empty() == true);

  buf.append("New data", 8);
  XNET_ASSERT(buf.size() == 8);
  XNET_ASSERT(std::memcmp(buf.data(), "New data", 8) == 0);
}

/** @brief Tests the correctness of multiple sequential appends using both
 *        append(const char*, size_t) and append(char) overloads,
 *        verifying the accumulated size and element-wise content.
 */
XNET_TEST(MultipleAppends) {
  Buffer buf;
  buf.append("A", 1);
  buf.append("BB", 2);
  buf.append("CCC", 3);
  buf.append("DDDD", 4);
  buf.append('E');
  buf.append("FFFFF", 5);

  XNET_ASSERT(buf.size() == 1 + 2 + 3 + 4 + 1 + 5);
  XNET_ASSERT(buf.size() == 16);
  XNET_ASSERT(std::memcmp(buf.data(), "ABBCCCDDDDEFFFFF", 16) == 0);
  XNET_ASSERT(buf.data()[0] == 'A');
  XNET_ASSERT(buf.data()[3] == 'C');
  XNET_ASSERT(buf.data()[6] == 'D');
  XNET_ASSERT(buf.data()[10] == 'E');
  XNET_ASSERT(buf.data()[11] == 'F');
}

/** @brief 创建 Buffer 并 append 数据，clone() 后验证深度拷贝且修改不影响原
 * Buffer。
 */
XNET_TEST(CloneBuffer) {
  Buffer buf;
  buf.append("Hello, Buffer!", 14);
  XNET_ASSERT(buf.size() == 14);

  Buffer cloned = buf.clone();
  XNET_ASSERT(cloned.size() == 14);
  XNET_ASSERT(cloned.data() != buf.data());
  XNET_ASSERT(std::memcmp(cloned.data(), buf.data(), 14) == 0);

  buf.append(" Modified", 9);
  XNET_ASSERT(buf.size() == 23);
  XNET_ASSERT(cloned.size() == 14);
  XNET_ASSERT(std::memcmp(cloned.data(), "Hello, Buffer!", 14) == 0);
  XNET_ASSERT(std::memcmp(buf.data(), "Hello, Buffer! Modified", 23) == 0);
}

/** @brief 移动构造后原 Buffer 为空，新 Buffer 持有数据；移动赋值同理。
 */
XNET_TEST(MoveSemantics) {
  // 移动构造
  Buffer buf_a;
  buf_a.append("Move me!", 8);
  XNET_ASSERT(buf_a.size() == 8);
  XNET_ASSERT(buf_a.data() != nullptr);

  Buffer buf_b(std::move(buf_a));
  XNET_ASSERT(buf_b.size() == 8);
  XNET_ASSERT(std::memcmp(buf_b.data(), "Move me!", 8) == 0);
  XNET_ASSERT(buf_a.size() == 0);
  XNET_ASSERT(buf_a.data() == nullptr);
  XNET_ASSERT(buf_a.empty() == true);

  // 移动赋值
  Buffer buf_c;
  buf_c.append("Destination", 11);
  Buffer buf_d;
  buf_d.append("Source", 6);

  buf_c = std::move(buf_d);
  XNET_ASSERT(buf_c.size() == 6);
  XNET_ASSERT(std::memcmp(buf_c.data(), "Source", 6) == 0);
  XNET_ASSERT(buf_d.size() == 0);
  XNET_ASSERT(buf_d.data() == nullptr);
  XNET_ASSERT(buf_d.empty() == true);

  // 移动赋值（接收一个空 Buffer）
  Buffer buf_e;
  buf_c = std::move(buf_e);
  XNET_ASSERT(buf_c.size() == 0);
  XNET_ASSERT(buf_c.data() == nullptr);
  XNET_ASSERT(buf_c.empty() == true);
}

/** @brief 创建一个 MallocAllocator，传入 Buffer 构造，append
 * 数据后验证正常工作。
 */
XNET_TEST(CustomAllocator) {
  MallocAllocator alloc;
  Buffer buf(&alloc);
  XNET_ASSERT(buf.data() == nullptr);
  XNET_ASSERT(buf.size() == 0);
  XNET_ASSERT(buf.empty() == true);

  buf.append("Custom allocator test", 21);
  XNET_ASSERT(buf.size() == 21);
  XNET_ASSERT(buf.data() != nullptr);
  XNET_ASSERT(std::memcmp(buf.data(), "Custom allocator test", 21) == 0);

  buf.append('!');
  XNET_ASSERT(buf.size() == 22);
  XNET_ASSERT(buf.data()[21] == '!');

  buf.clear();
  XNET_ASSERT(buf.size() == 0);
  XNET_ASSERT(buf.empty() == true);

  buf.append("Reuse after clear", 18);
  XNET_ASSERT(buf.size() == 18);
  XNET_ASSERT(std::memcmp(buf.data(), "Reuse after clear", 18) == 0);
}

/** @brief 用新的 find(char) 方法查找单个字符，包括存在、不存在、空 buffer。
 */
XNET_TEST(FindChar) {
  Buffer buf;
  const char data[] = "The quick brown fox jumps over the lazy dog.";
  buf.append(data, std::strlen(data));

  // 字符存在
  XNET_ASSERT(buf.find('T') == 0);
  XNET_ASSERT(buf.find('q') == 4);
  XNET_ASSERT(buf.find('e') == 2);
  XNET_ASSERT(buf.find('x') == 18);
  XNET_ASSERT(buf.find('g') == 42);
  XNET_ASSERT(buf.find('.') == 43);

  // 字符不存在
  XNET_ASSERT(buf.find('Z') == Buffer::npos);
  XNET_ASSERT(buf.find('1') == Buffer::npos);

  // 空 buffer 查找
  Buffer empty_buf;
  XNET_ASSERT(empty_buf.find('a') == Buffer::npos);
  XNET_ASSERT(empty_buf.find('\0') == Buffer::npos);

  // 单字符 buffer
  Buffer single;
  single.append("X", 1);
  XNET_ASSERT(single.find('X') == 0);
  XNET_ASSERT(single.find('x') == Buffer::npos);
}

/** @brief Test runner entry point. Executes all XNET_TEST cases defined in this
 *        file and prints a summary message upon completion.
 */
int main() {
  XNET_RUN_TEST(DefaultConstruction);
  XNET_RUN_TEST(AppendBytes);
  XNET_RUN_TEST(AppendChar);
  XNET_RUN_TEST(PopFront);
  XNET_RUN_TEST(ReserveAndCapacity);
  XNET_RUN_TEST(Resize);
  XNET_RUN_TEST(Swap);
  XNET_RUN_TEST(FindSubstring);
  XNET_RUN_TEST(ClearResetsToEmpty);
  XNET_RUN_TEST(MultipleAppends);
  XNET_RUN_TEST(CloneBuffer);
  XNET_RUN_TEST(MoveSemantics);
  XNET_RUN_TEST(CustomAllocator);
  XNET_RUN_TEST(FindChar);

  printf("All tests completed.\n");
  return 0;
}
