/**
 * @file test_utils.c
 * @brief Test utilities implementation
 */

#include "test_utils.h"

int test_count = 0;
int test_passed = 0;
int test_failed = 0;

void test_init(void) {
    test_count = 0;
    test_passed = 0;
    test_failed = 0;
}

void test_cleanup(void) {
    /* Nothing to cleanup for now */
}