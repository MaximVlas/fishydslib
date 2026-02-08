/**
 * @file test_string.c
 * @brief String tests (placeholder)
 */

#include "test_utils.h"
#include "core/dc_string.h"

int test_string_main(void) {
    TEST_SUITE_BEGIN("String Tests");

    dc_string_t str;
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&str), "dc_string_init");
    TEST_ASSERT_EQ(0u, dc_string_length(&str), "empty length");
    TEST_ASSERT_STR_EQ("", dc_string_cstr(&str), "empty cstr");

    TEST_ASSERT_EQ(DC_OK, dc_string_append_cstr(&str, "Hello"), "append cstr");
    TEST_ASSERT_EQ(5u, dc_string_length(&str), "length after append");
    TEST_ASSERT_STR_EQ("Hello", dc_string_cstr(&str), "content after append");

    TEST_ASSERT_EQ(DC_OK, dc_string_append_char(&str, ' '), "append char");
    TEST_ASSERT_EQ(DC_OK, dc_string_append_buffer(&str, "World", (size_t)5), "append buffer");
    TEST_ASSERT_STR_EQ("Hello World", dc_string_cstr(&str), "content after buffer");

    TEST_ASSERT_EQ(DC_OK, dc_string_append_printf(&str, " %d", 123), "append printf");
    TEST_ASSERT_STR_EQ("Hello World 123", dc_string_cstr(&str), "content after printf");

    TEST_ASSERT_EQ(DC_OK, dc_string_set_cstr(&str, "Reset"), "set cstr");
    TEST_ASSERT_STR_EQ("Reset", dc_string_cstr(&str), "content after set");

    TEST_ASSERT_EQ(DC_OK, dc_string_clear(&str), "clear");
    TEST_ASSERT_EQ(0u, dc_string_length(&str), "length after clear");
    TEST_ASSERT_STR_EQ("", dc_string_cstr(&str), "content after clear");

    TEST_ASSERT_EQ(DC_OK, dc_string_reserve(&str, (size_t)64), "reserve");
    TEST_ASSERT(dc_string_capacity(&str) >= 64u, "capacity after reserve");

    TEST_ASSERT_EQ(DC_OK, dc_string_append_cstr(&str, "abc"), "append after reserve");
    TEST_ASSERT_EQ(DC_OK, dc_string_shrink_to_fit(&str), "shrink to fit");
    TEST_ASSERT_EQ(dc_string_length(&str) + 1u, dc_string_capacity(&str), "capacity after shrink");

    dc_string_free(&str);

    TEST_SUITE_END("String Tests");
}
