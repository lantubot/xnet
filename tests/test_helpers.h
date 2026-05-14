/** @file test_helpers.h
 * @brief 最小化测试框架辅助工具（无 STL，无异常）。
 *        提供断言宏、字符串比较宏、测试注册宏及单次测试运行宏，
 *        所有测试状态由全局变量追踪，测试结果通过 printf 输出。
 */

#pragma once
#ifndef XNET_TEST_HELPERS_H_
#define XNET_TEST_HELPERS_H_

#include <cstddef>  // size_t
#include <cstdio>   // printf
#include <cstring>  // strcmp

/** @brief 测试命名空间。包含测试框架的核心变量和函数。 */
namespace xnet {
namespace test {

/** @brief 已通过的断言总数。 */
inline int g_tests_passed = 0;
/** @brief 已失败的断言总数。 */
inline int g_tests_failed = 0;
/** @brief 当前正在执行的测试名称指针。由 test_begin() 设置，用于错误信息。 */
inline const char* g_current_test = "";

/** @brief 断言条件表达式。若条件为假则记录失败，否则记录通过。
 * @param condition 要测试的布尔条件。
 * @param expr      条件的字符串表达式（用于错误输出）。
 * @param file      断言发生的源文件名（通常使用 __FILE__）。
 * @param line      断言发生的行号（通常使用 __LINE__）。
 */
inline void test_assert(bool condition, const char* expr, const char* file,
                        int line) {
  if (!condition) {
    printf("  FAIL: %s (%s:%d)\n", expr, file, line);
    g_tests_failed++;
  } else {
    g_tests_passed++;
  }
}

/** @brief 断言两个 C
 * 风格字符串相等。若任一为空或不相等则记录失败，否则记录通过。
 * @param expected 期望的字符串。
 * @param actual   实际的字符串。
 * @param file     断言发生的源文件名（通常使用 __FILE__）。
 * @param line     断言发生的行号（通常使用 __LINE__）。
 */
inline void test_assert_str_eq(const char* expected, const char* actual,
                               const char* file, int line) {
  if (!expected || !actual || std::strcmp(expected, actual) != 0) {
    printf("  FAIL: expected \"%s\", got \"%s\" (%s:%d)\n",
           expected ? expected : "null", actual ? actual : "null", file, line);
    g_tests_failed++;
  } else {
    g_tests_passed++;
  }
}

/** @brief 开始执行一个测试用例。设置当前测试名称并打印 [ RUN ] 标记。
 * @param name 测试用例名称。
 */
inline void test_begin(const char* name) {
  g_current_test = name;
  printf("[ RUN  ] %s\n", name);
}

/** @brief 结束一个测试用例。根据测试结果打印 [ OK ] 或 [ FAIL ] 标记，
 *        重置全局计数器，并返回失败数。
 * @param name 测试用例名称。
 * @return 失败断言的数量（0 表示全部通过）。
 */
inline int test_end(const char* name) {
  if (g_tests_failed > 0) {
    printf("[ FAIL ] %s (%d assertions, %d failed)\n\n", name,
           g_tests_passed + g_tests_failed, g_tests_failed);
    int failed = g_tests_failed;
    g_tests_failed = 0;
    g_tests_passed = 0;
    return failed;
  }
  printf("[  OK  ] %s (%d assertions)\n\n", name, g_tests_passed);
  g_tests_passed = 0;
  return 0;
}

}  // namespace test
}  // namespace xnet

/** @def XNET_TEST(name)
 * @brief 注册并定义一个测试用例。
 *        展开为一个前向声明、一个 run_test_##name() 入口函数，
 *        以及一个空的静态函数体供用户填充断言。
 * @param name 测试用例的唯一标识符（标识符形式，不含引号）。
 */
#define XNET_TEST(name)                   \
  static void xnet_test_##name();         \
  int run_test_##name() {                 \
    ::xnet::test::test_begin(#name);      \
    xnet_test_##name();                   \
    return ::xnet::test::test_end(#name); \
  }                                       \
  static void xnet_test_##name()

/** @def XNET_ASSERT(cond)
 * @brief 断言条件为真。若条件为假则记录失败并打印表达式和位置。
 * @param cond 要断言的布尔表达式。会被双重否定（!!）以确保布尔语义。
 */
#define XNET_ASSERT(cond) \
  ::xnet::test::test_assert(!!(cond), #cond, __FILE__, __LINE__)

/** @def XNET_ASSERT_STR_EQ(expected, actual)
 * @brief 断言两个 C 风格字符串相等。不相等时打印两个值及位置。
 * @param expected 期望的字符串表达式。
 * @param actual   实际的字符串表达式。
 */
#define XNET_ASSERT_STR_EQ(expected, actual) \
  ::xnet::test::test_assert_str_eq(expected, actual, __FILE__, __LINE__)

/** @def XNET_RUN_TEST(name)
 * @brief 运行一个已注册的测试用例。若测试失败则从当前函数返回非零值。
 * @param name 测试用例的唯一标识符（与 XNET_TEST 传入的 name 一致）。
 */
#define XNET_RUN_TEST(name)      \
  do {                           \
    int ret = run_test_##name(); \
    if (ret != 0) return ret;    \
  } while (0)

#endif  // XNET_TEST_HELPERS_H_
