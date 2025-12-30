// tests/forge_test.h - Minimal C Unit Test Framework
#ifndef FORGE_TEST_H
#define FORGE_TEST_H

#include <stdio.h>
#include <stdbool.h>

#define TEST(name) void test_##name(void)
#define RUN_TEST(name)        \
  do                          \
  {                           \
    printf("  ✓ %s ", #name); \
    test_##name();            \
    printf("PASS\n");         \
  } while (0)

#define ASSERT_TRUE(cond)                          \
  do                                               \
  {                                                \
    if (!(cond))                                   \
    {                                              \
      printf("FAIL: %s:%d\n", __FILE__, __LINE__); \
      return;                                      \
    }                                              \
  } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQUAL(a, b)                                                                \
  do                                                                                      \
  {                                                                                       \
    if ((a) != (b))                                                                       \
    {                                                                                     \
      printf("FAIL: %s:%d expected %d got %d\n", __FILE__, __LINE__, (int)(a), (int)(b)); \
      return;                                                                             \
    }                                                                                     \
  } while (0)

#define ASSERT_STR_EQUAL(a, b)                                                  \
  do                                                                            \
  {                                                                             \
    if (strcmp(a, b) != 0)                                                      \
    {                                                                           \
      printf("FAIL: %s:%d expected '%s' got '%s'\n", __FILE__, __LINE__, b, a); \
      return;                                                                   \
    }                                                                           \
  } while (0)

#endif
