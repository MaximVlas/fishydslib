#ifndef TEST_UTILS_H
#define TEST_UTILS_H

/**
 * @file test_utils.h
 * @brief Test utilities and macros
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Test result tracking */
extern int test_count;
extern int test_passed;
extern int test_failed;

/* Test macros */
#define TEST_ASSERT(condition, message) \
    do { \
        test_count++; \
        if (condition) { \
            test_passed++; \
            printf("PASS: %s\n", message); \
        } else { \
            test_failed++; \
            printf("FAIL: %s (line %d)\n", message, __LINE__); \
        } \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

#define TEST_ASSERT_NEQ(expected, actual, message) \
    TEST_ASSERT((expected) != (actual), message)

#define TEST_ASSERT_STR_EQ(expected, actual, message) \
    TEST_ASSERT(strcmp((expected), (actual)) == 0, message)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

#define TEST_SUITE_BEGIN(name) \
    printf("\n=== Running %s ===\n", name); \
    test_count = 0; \
    test_passed = 0; \
    test_failed = 0;

#define TEST_SUITE_END(name) \
    printf("\n=== %s Results ===\n", name); \
    printf("Total: %d, Passed: %d, Failed: %d\n", test_count, test_passed, test_failed); \
    if (test_failed > 0) { \
        printf("SUITE FAILED\n"); \
        return 1; \
    } else { \
        printf("SUITE PASSED\n"); \
        return 0; \
    }

/* Test utility functions */
void test_init(void);
void test_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_UTILS_H */