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

#include "test_helpers.h"
#include "xnet/error.h"

using namespace xnet;

/** @brief 验证所有 Status 枚举值的 to_string() 返回值正确。 */
XNET_TEST(StatusToString) {
  XNET_ASSERT_STR_EQ("OK", to_string(Status::OK));
  XNET_ASSERT_STR_EQ("UNKNOWN", to_string(Status::UNKNOWN));
  XNET_ASSERT_STR_EQ("INVALID_ARGUMENT", to_string(Status::INVALID_ARGUMENT));
  XNET_ASSERT_STR_EQ("NOT_FOUND", to_string(Status::NOT_FOUND));
  XNET_ASSERT_STR_EQ("TIMEOUT", to_string(Status::TIMEOUT));
  XNET_ASSERT_STR_EQ("CONNECTION_REFUSED",
                     to_string(Status::CONNECTION_REFUSED));
  XNET_ASSERT_STR_EQ("CONNECTION_RESET", to_string(Status::CONNECTION_RESET));
  XNET_ASSERT_STR_EQ("DNS_FAILURE", to_string(Status::DNS_FAILURE));
  XNET_ASSERT_STR_EQ("PROTOCOL_ERROR", to_string(Status::PROTOCOL_ERROR));
  XNET_ASSERT_STR_EQ("OUT_OF_MEMORY", to_string(Status::OUT_OF_MEMORY));
  XNET_ASSERT_STR_EQ("UNSUPPORTED_PROTOCOL",
                     to_string(Status::UNSUPPORTED_PROTOCOL));
  XNET_ASSERT_STR_EQ("SSL_ERROR", to_string(Status::SSL_ERROR));
  XNET_ASSERT_STR_EQ("IO_ERROR", to_string(Status::IO_ERROR));
}

/** @brief 测试仅使用 Status 码构造 Error，验证 what() 返回 to_string(code)。 */
XNET_TEST(ErrorWithCode) {
  Error e1(Status::NOT_FOUND);
  XNET_ASSERT(e1.code == Status::NOT_FOUND);
  XNET_ASSERT(e1.message == nullptr);
  XNET_ASSERT_STR_EQ("NOT_FOUND", e1.what());

  Error e2(Status::IO_ERROR);
  XNET_ASSERT(e2.code == Status::IO_ERROR);
  XNET_ASSERT_STR_EQ("IO_ERROR", e2.what());

  Error e3(Status::OK);
  XNET_ASSERT(e3.code == Status::OK);
  XNET_ASSERT_STR_EQ("OK", e3.what());
}

/** @brief 测试使用 Status 码和自定义消息构造 Error，验证 what()
 * 返回自定义消息。 */
XNET_TEST(ErrorWithMessage) {
  Error e1(Status::TIMEOUT, "connection timed out after 30s");
  XNET_ASSERT(e1.code == Status::TIMEOUT);
  XNET_ASSERT_STR_EQ("connection timed out after 30s", e1.what());

  Error e2(Status::DNS_FAILURE, "hostname not found");
  XNET_ASSERT(e2.code == Status::DNS_FAILURE);
  XNET_ASSERT_STR_EQ("hostname not found", e2.what());

  Error e3(Status::OK, "all good");
  XNET_ASSERT_STR_EQ("all good", e3.what());
}

/** @brief 测试 Result::ok(val) 构造函数，验证 value() 正确且 is_ok() 返回
 * true。 */
XNET_TEST(ResultOk) {
  Result<int> r = Result<int>::ok(42);
  XNET_ASSERT(r.is_ok() == true);
  XNET_ASSERT(r.is_err() == false);
  XNET_ASSERT(r.value() == 42);

  Result<int> r2 = Result<int>::ok(-1);
  XNET_ASSERT(r2.is_ok() == true);
  XNET_ASSERT(r2.value() == -1);
}

/** @brief 测试 Result::err(error) 构造函数，验证 is_err() 返回 true 且 error()
 * 正确。 */
XNET_TEST(ResultErr) {
  Error err(Status::NOT_FOUND, "resource missing");
  Result<int> r = Result<int>::err(err);
  XNET_ASSERT(r.is_ok() == false);
  XNET_ASSERT(r.is_err() == true);
  XNET_ASSERT(r.error().code == Status::NOT_FOUND);
  XNET_ASSERT_STR_EQ("resource missing", r.error().what());

  Result<int> r2 = Result<int>::err(Error(Status::TIMEOUT));
  XNET_ASSERT(r2.is_err() == true);
  XNET_ASSERT_STR_EQ("TIMEOUT", r2.error().what());
}

/** @brief 测试 Result 的拷贝构造和拷贝赋值操作。 */
XNET_TEST(ResultCopy) {
  // 拷贝构造（ok 状态）
  Result<int> r1 = Result<int>::ok(100);
  Result<int> r2(r1);
  XNET_ASSERT(r1.is_ok() == true);
  XNET_ASSERT(r1.value() == 100);
  XNET_ASSERT(r2.is_ok() == true);
  XNET_ASSERT(r2.value() == 100);

  // 拷贝构造（err 状态）
  Result<int> r3 =
      Result<int>::err(Error(Status::INVALID_ARGUMENT, "bad input"));
  Result<int> r4(r3);
  XNET_ASSERT(r3.is_err() == true);
  XNET_ASSERT_STR_EQ("bad input", r3.error().what());
  XNET_ASSERT(r4.is_err() == true);
  XNET_ASSERT_STR_EQ("bad input", r4.error().what());

  // 拷贝赋值（ok -> ok）
  Result<int> r5 = Result<int>::ok(1);
  Result<int> r6 = Result<int>::ok(2);
  r6 = r5;
  XNET_ASSERT(r6.is_ok() == true);
  XNET_ASSERT(r6.value() == 1);

  // 拷贝赋值（err -> ok）
  Result<int> r7 = Result<int>::ok(999);
  Result<int> r8 = Result<int>::err(Error(Status::IO_ERROR));
  r7 = r8;
  XNET_ASSERT(r7.is_err() == true);
  XNET_ASSERT(r7.error().code == Status::IO_ERROR);

  // 拷贝赋值（ok -> err）
  Result<int> r9 = Result<int>::err(Error(Status::NOT_FOUND));
  Result<int> r10 = Result<int>::ok(77);
  r9 = r10;
  XNET_ASSERT(r9.is_ok() == true);
  XNET_ASSERT(r9.value() == 77);
}

/** @brief 测试 Result 的移动构造和移动赋值操作。 */
XNET_TEST(ResultMove) {
  // 移动构造（ok 状态）
  Result<int> r1 = Result<int>::ok(200);
  Result<int> r2(static_cast<Result<int>&&>(r1));
  XNET_ASSERT(r2.is_ok() == true);
  XNET_ASSERT(r2.value() == 200);

  // 移动构造（err 状态）
  Result<int> r3 =
      Result<int>::err(Error(Status::CONNECTION_REFUSED, "refused"));
  Result<int> r4(static_cast<Result<int>&&>(r3));
  XNET_ASSERT(r4.is_err() == true);
  XNET_ASSERT(r4.error().code == Status::CONNECTION_REFUSED);
  XNET_ASSERT_STR_EQ("refused", r4.error().what());

  // 移动赋值（ok -> ok）
  Result<int> r5 = Result<int>::ok(10);
  Result<int> r6 = Result<int>::ok(20);
  r6 = static_cast<Result<int>&&>(r5);
  XNET_ASSERT(r6.is_ok() == true);
  XNET_ASSERT(r6.value() == 10);

  // 移动赋值（err -> ok）
  Result<int> r7 = Result<int>::ok(42);
  Result<int> r8 = Result<int>::err(Error(Status::PROTOCOL_ERROR));
  r7 = static_cast<Result<int>&&>(r8);
  XNET_ASSERT(r7.is_err() == true);
  XNET_ASSERT(r7.error().code == Status::PROTOCOL_ERROR);

  // 移动赋值（ok -> err）
  Result<int> r9 = Result<int>::err(Error(Status::OUT_OF_MEMORY));
  Result<int> r10 = Result<int>::ok(88);
  r9 = static_cast<Result<int>&&>(r10);
  XNET_ASSERT(r9.is_ok() == true);
  XNET_ASSERT(r9.value() == 88);
}

/** @brief 使用 Result<int> 测试各种值（正数、负数、零、边界值）。 */
XNET_TEST(ResultInt) {
  // 正数
  Result<int> r1 = Result<int>::ok(1);
  XNET_ASSERT(r1.is_ok() == true);
  XNET_ASSERT(r1.value() == 1);

  // 负数
  Result<int> r2 = Result<int>::ok(-1);
  XNET_ASSERT(r2.is_ok() == true);
  XNET_ASSERT(r2.value() == -1);

  // 零
  Result<int> r3 = Result<int>::ok(0);
  XNET_ASSERT(r3.is_ok() == true);
  XNET_ASSERT(r3.value() == 0);

  // 大正数
  Result<int> r4 = Result<int>::ok(2147483647);
  XNET_ASSERT(r4.is_ok() == true);
  XNET_ASSERT(r4.value() == 2147483647);

  // 大负数
  Result<int> r5 = Result<int>::ok(-2147483647 - 1);
  XNET_ASSERT(r5.is_ok() == true);
  XNET_ASSERT(r5.value() == -2147483647 - 1);

  // 错误状态
  Result<int> r6 = Result<int>::err(Error(Status::UNSUPPORTED_PROTOCOL));
  XNET_ASSERT(r6.is_err() == true);
  XNET_ASSERT(r6.error().code == Status::UNSUPPORTED_PROTOCOL);

  // 带消息的错误
  Result<int> r7 = Result<int>::err(
      Error(Status::SSL_ERROR, "certificate verification failed"));
  XNET_ASSERT(r7.is_err() == true);
  XNET_ASSERT(r7.error().code == Status::SSL_ERROR);
  XNET_ASSERT_STR_EQ("certificate verification failed", r7.error().what());

  // 从 err 状态拷贝
  Result<int> r8(r6);
  XNET_ASSERT(r8.is_err() == true);
  XNET_ASSERT(r8.error().code == Status::UNSUPPORTED_PROTOCOL);
}

/** @brief 测试运行入口。依次执行所有测试用例并输出汇总信息。 */
int main() {
  XNET_RUN_TEST(StatusToString);
  XNET_RUN_TEST(ErrorWithCode);
  XNET_RUN_TEST(ErrorWithMessage);
  XNET_RUN_TEST(ResultOk);
  XNET_RUN_TEST(ResultErr);
  XNET_RUN_TEST(ResultCopy);
  XNET_RUN_TEST(ResultMove);
  XNET_RUN_TEST(ResultInt);

  printf("All tests completed.\n");
  return 0;
}
