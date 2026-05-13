// XNet test helpers — minimal test framework (no STL, no exceptions)
//
// Usage:
//   XNET_TEST(test_name) {
//     XNET_ASSERT(1 + 1 == 2);
//     XNET_ASSERT_STR_EQ("hello", "hello");
//   }

#pragma once
#ifndef XNET_TEST_HELPERS_H_
#define XNET_TEST_HELPERS_H_

#include <cstdio>   // printf
#include <cstddef>  // size_t
#include <cstring>  // strcmp

namespace xnet {
namespace test {

// Test state
inline int g_tests_passed = 0;
inline int g_tests_failed = 0;
inline const char* g_current_test = "";

inline void test_assert(bool condition, const char* expr,
                         const char* file, int line) {
  if (!condition) {
    printf("  FAIL: %s (%s:%d)\n", expr, file, line);
    g_tests_failed++;
  } else {
    g_tests_passed++;
  }
}

inline void test_assert_str_eq(const char* expected, const char* actual,
                                const char* file, int line) {
  if (!expected || !actual || std::strcmp(expected, actual) != 0) {
    printf("  FAIL: expected \"%s\", got \"%s\" (%s:%d)\n",
           expected ? expected : "null",
           actual ? actual : "null", file, line);
    g_tests_failed++;
  } else {
    g_tests_passed++;
  }
}

inline void test_begin(const char* name) {
  g_current_test = name;
  printf("[ RUN  ] %s\n", name);
}

inline int test_end(const char* name) {
  if (g_tests_failed > 0) {
    printf("[ FAIL ] %s (%d assertions, %d failed)\n\n",
           name, g_tests_passed + g_tests_failed, g_tests_failed);
    int failed = g_tests_failed;
    g_tests_failed = 0;
    g_tests_passed = 0;
    return failed;
  }
  printf("[  OK  ] %s (%d assertions)\n\n",
         name, g_tests_passed);
  g_tests_passed = 0;
  return 0;
}

}  // namespace test
}  // namespace xnet

#define XNET_TEST(name) \
  static void xnet_test_##name(); \
  int run_test_##name() { \
    ::xnet::test::test_begin(#name); \
    xnet_test_##name(); \
    return ::xnet::test::test_end(#name); \
  } \
  static void xnet_test_##name()

#define XNET_ASSERT(cond) \
  ::xnet::test::test_assert(!!(cond), #cond, __FILE__, __LINE__)

#define XNET_ASSERT_STR_EQ(expected, actual) \
  ::xnet::test::test_assert_str_eq(expected, actual, __FILE__, __LINE__)

#define XNET_RUN_TEST(name) do { \
  int ret = run_test_##name(); \
  if (ret != 0) return ret; \
} while (0)

#endif  // XNET_TEST_HELPERS_H_
