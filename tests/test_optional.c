/**
 * @file test_optional.c
 * @brief Optional/nullable helper tests
 */

#include "test_utils.h"
#include "core/dc_optional.h"

int test_optional_main(void) {
    TEST_SUITE_BEGIN("Optional/Nullable Tests");

    DC_OPTIONAL(int) opt = {0};
    TEST_ASSERT_EQ(0, opt.is_set, "optional init is_set");
    DC_OPTIONAL_SET(opt, 42);
    TEST_ASSERT_EQ(1, opt.is_set, "optional set");
    TEST_ASSERT_EQ(42, opt.value, "optional value");
    DC_OPTIONAL_CLEAR(opt);
    TEST_ASSERT_EQ(0, opt.is_set, "optional clear");

    DC_NULLABLE(int) nul = {0};
    nul.is_null = 1;
    TEST_ASSERT_EQ(1, nul.is_null, "nullable init is_null");
    DC_NULLABLE_SET(nul, 7);
    TEST_ASSERT_EQ(0, nul.is_null, "nullable set");
    TEST_ASSERT_EQ(7, nul.value, "nullable value");
    DC_NULLABLE_SET_NULL(nul);
    TEST_ASSERT_EQ(1, nul.is_null, "nullable set null");

    TEST_SUITE_END("Optional/Nullable Tests");
}
