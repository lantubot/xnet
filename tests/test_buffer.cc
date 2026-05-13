// Copyright (c) 2026 XNet Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// xnet::Buffer 的单元测试。
//
// 测试用例：
//  1. 默认构造（null, size=0）
//  2. 追加字节并检查大小/数据
//  3. 追加单个字符
//  4. pop_front
//  5. reserve 和 capacity
//  6. resize
//  7. swap
//  8. 在缓冲区中查找子串
//  9. clear 重置为空
// 10. 多次追加

#include "test_helpers.h"
#include "xnet/buffer.h"

#include <cstring>  // strlen, memcmp

using namespace xnet;
// =============================================================================
// 1. 默认构造 — null 数据，size == 0
// =============================================================================
XNET_TEST(DefaultConstruction) {
  Buffer buf;

  XNET_ASSERT(buf.data() == nullptr);
  XNET_ASSERT(buf.size() == 0);
  XNET_ASSERT(buf.capacity() == 0);
  XNET_ASSERT(buf.empty() == true);
}

// =============================================================================
// 2. 追加原始字节并检查大小 / 数据
// =============================================================================
XNET_TEST(AppendBytes) {
  Buffer buf;

  const char hello[] = "Hello, Buffer!";
  buf.append(hello, sizeof(hello) - 1);  // exclude null terminator

  XNET_ASSERT(buf.size() == 14);
  XNET_ASSERT(buf.empty() == false);
  XNET_ASSERT(buf.data() != nullptr);
  XNET_ASSERT(std::memcmp(buf.data(), hello, 14) == 0);

  // 向已有数据的缓冲区追加更多字节
  const char more[] = " More data.";
  buf.append(more, 10);
  XNET_ASSERT(buf.size() == 24);
  XNET_ASSERT(std::memcmp(buf.data(), "Hello, Buffer! More data.", 24) == 0);
}

// =============================================================================
// 3. 追加单个字符
// =============================================================================
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

// =============================================================================
// 4. pop_front — 从头部移除字节
// =============================================================================
XNET_TEST(PopFront) {
  Buffer buf;
  const char data[] = "ABCDEFGHIJ";
  buf.append(data, 10);

  // pop_front 时 n == 0 — 无变化
  buf.pop_front(0);
  XNET_ASSERT(buf.size() == 10);

  // 部分 pop_front
  buf.pop_front(3);
  XNET_ASSERT(buf.size() == 7);
  XNET_ASSERT(std::memcmp(buf.data(), "DEFGHIJ", 7) == 0);

  // 移除所有剩余数据
  buf.pop_front(7);
  XNET_ASSERT(buf.size() == 0);
  XNET_ASSERT(buf.empty() == true);

  // 超出末尾的 pop_front — 保持在 0
  buf.pop_front(100);
  XNET_ASSERT(buf.size() == 0);
}

// =============================================================================
// 5. reserve 和 capacity
// =============================================================================
XNET_TEST(ReserveAndCapacity) {
  Buffer buf;

  XNET_ASSERT(buf.capacity() == 0);

  buf.reserve(64);
  XNET_ASSERT(buf.capacity() >= 64);   // 具体值由实现定义
  XNET_ASSERT(buf.data() != nullptr);
  XNET_ASSERT(buf.size() == 0);        // 大小不变

  // reserve 更小的容量是空操作
  buf.reserve(32);
  XNET_ASSERT(buf.capacity() >= 64);   // 不变

  // 更大的 reserve 会实际扩容
  buf.reserve(128);
  XNET_ASSERT(buf.capacity() >= 128);

  // reserve 之后，仍可正常追加数据
  buf.append("hello", 5);
  XNET_ASSERT(buf.size() == 5);
  XNET_ASSERT(std::memcmp(buf.data(), "hello", 5) == 0);
}

// =============================================================================
// 6. resize
// =============================================================================
XNET_TEST(Resize) {
  Buffer buf;

  // 从空缓冲区 resize 扩大 — 零填充
  buf.resize(10);
  XNET_ASSERT(buf.size() == 10);
  XNET_ASSERT(buf.capacity() >= 10);
  for (size_t i = 0; i < 10; ++i) {
    XNET_ASSERT(buf.data()[i] == 0);
  }

  // 填充一些数据
  std::memcpy(buf.data(), "ABCDE", 5);

  // 缩小 — 前 3 个字节保留
  buf.resize(3);
  XNET_ASSERT(buf.size() == 3);
  XNET_ASSERT(std::memcmp(buf.data(), "ABC", 3) == 0);

  // 再次扩大 — 新字节零填充，旧字节保留
  buf.resize(8);
  XNET_ASSERT(buf.size() == 8);
  XNET_ASSERT(std::memcmp(buf.data(), "ABC", 3) == 0);
  XNET_ASSERT(buf.data()[3] == 0);
  XNET_ASSERT(buf.data()[7] == 0);
}

// =============================================================================
// 7. swap
// =============================================================================
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

  // 与空缓冲区交换
  Buffer buf_c;
  buf_a.swap(buf_c);

  XNET_ASSERT(buf_a.size() == 0);
  XNET_ASSERT(buf_a.data() == nullptr);
  XNET_ASSERT(buf_c.size() == 7);
  XNET_ASSERT(std::memcmp(buf_c.data(), "BBBBBBB", 7) == 0);
}

// =============================================================================
// 8. 在缓冲区中查找子串
// =============================================================================
XNET_TEST(FindSubstring) {
  Buffer buf;
  const char data[] = "The quick brown fox jumps over the lazy dog.";
  buf.append(data, std::strlen(data));

  // 子串出现在开头
  size_t pos = buf.find("The", 3);
  XNET_ASSERT(pos == 0);

  // 子串在中间
  pos = buf.find("fox", 3);
  XNET_ASSERT(pos == 16);

  // 子串靠近末尾
  pos = buf.find("dog", 3);
  XNET_ASSERT(pos == 40);

  // 子串未找到
  pos = buf.find("cat", 3);
  XNET_ASSERT(pos == Buffer::npos);

  // 空 needle 返回 0
  pos = buf.find("", 0);
  XNET_ASSERT(pos == 0);

  // needle 长于缓冲区
  pos = buf.find("The quick brown fox jumps over the lazy dog. Extra", 49);
  XNET_ASSERT(pos == Buffer::npos);

  // 查找单个字符
  pos = buf.find("q", 1);
  XNET_ASSERT(pos == 4);
}

// =============================================================================
// 9. clear 重置为空
// =============================================================================
XNET_TEST(ClearResetsToEmpty) {
  Buffer buf;
  buf.append("Hello, world!", 13);

  XNET_ASSERT(buf.size() == 13);
  XNET_ASSERT(buf.empty() == false);

  buf.clear();

  XNET_ASSERT(buf.size() == 0);
  XNET_ASSERT(buf.empty() == true);

  // 数据指针应保留（内存不释放），但我们不解引用已清除的字节。
  // clear 之后，可以像新缓冲区一样再次追加数据。
  buf.append("New data", 8);
  XNET_ASSERT(buf.size() == 8);
  XNET_ASSERT(std::memcmp(buf.data(), "New data", 8) == 0);
}

// =============================================================================
// 10. 多次追加 — 顺序追加操作正确复合
// =============================================================================
XNET_TEST(MultipleAppends) {
  Buffer buf;

  // 分批追加小数据块
  buf.append("A", 1);
  buf.append("BB", 2);
  buf.append("CCC", 3);
  buf.append("DDDD", 4);
  buf.append('E');
  buf.append("FFFFF", 5);

  XNET_ASSERT(buf.size() == 1 + 2 + 3 + 4 + 1 + 5);
  XNET_ASSERT(buf.size() == 16);

  // 验证精确内容
  const char expected[] = "ABBCCCDDDDEFFFFF";
  XNET_ASSERT(std::memcmp(buf.data(), expected, 16) == 0);

  // 验证各个位置的字符
  XNET_ASSERT(buf.data()[0] == 'A');
  XNET_ASSERT(buf.data()[3] == 'C');   // index 3 = start of "CCC"
  XNET_ASSERT(buf.data()[6] == 'D');   // index 6 = start of "DDDD"
  XNET_ASSERT(buf.data()[10] == 'E');
  XNET_ASSERT(buf.data()[11] == 'F');
}

// =============================================================================
// 主函数 — 运行所有已注册的测试
// =============================================================================
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
