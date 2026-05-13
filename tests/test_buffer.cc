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

#include "test_helpers.h"
#include "xnet/buffer.h"

using namespace xnet;

XNET_TEST(DefaultConstruction) {
  Buffer buf;
  XNET_ASSERT(buf.data() == nullptr);
  XNET_ASSERT(buf.size() == 0);
  XNET_ASSERT(buf.capacity() == 0);
  XNET_ASSERT(buf.empty() == true);
}

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

  printf("All tests completed.\n");
  return 0;
}
