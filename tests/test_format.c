/**
 * @file test_format.c
 * @brief Format tests
 */

#include "test_utils.h"
#include "core/dc_format.h"

int test_format_main(void) {
    TEST_SUITE_BEGIN("Format Tests");

    dc_string_t out;
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&out), "format init string");

    TEST_ASSERT_EQ(DC_OK, dc_format_mention_user(123, &out), "mention user");
    TEST_ASSERT_STR_EQ("<@123>", dc_string_cstr(&out), "mention user value");

    TEST_ASSERT_EQ(DC_OK, dc_format_mention_user_nick(123, &out), "mention user nick");
    TEST_ASSERT_STR_EQ("<@!123>", dc_string_cstr(&out), "mention user nick value");

    TEST_ASSERT_EQ(DC_OK, dc_format_mention_channel(456, &out), "mention channel");
    TEST_ASSERT_STR_EQ("<#456>", dc_string_cstr(&out), "mention channel value");

    TEST_ASSERT_EQ(DC_OK, dc_format_mention_role(789, &out), "mention role");
    TEST_ASSERT_STR_EQ("<@&789>", dc_string_cstr(&out), "mention role value");

    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_format_mention_user(0, &out), "mention invalid id");

    TEST_ASSERT_EQ(DC_OK, dc_format_slash_command_mention("ping", 42, &out), "slash mention");
    TEST_ASSERT_STR_EQ("</ping:42>", dc_string_cstr(&out), "slash mention value");
    TEST_ASSERT_EQ(DC_OK, dc_format_slash_command_mention("ping pong", 42, &out), "slash mention space");
    TEST_ASSERT_STR_EQ("</ping pong:42>", dc_string_cstr(&out), "slash mention space value");

    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_format_slash_command_mention("bad:name", 42, &out),
                   "slash mention invalid name");

    TEST_ASSERT(dc_format_timestamp_style_is_valid('R'), "timestamp style valid");
    TEST_ASSERT(!dc_format_timestamp_style_is_valid('x'), "timestamp style invalid");

    TEST_ASSERT_EQ(DC_OK, dc_format_timestamp(123, '\0', &out), "timestamp default");
    TEST_ASSERT_STR_EQ("<t:123>", dc_string_cstr(&out), "timestamp default value");

    TEST_ASSERT_EQ(DC_OK, dc_format_timestamp(123, 'R', &out), "timestamp style");
    TEST_ASSERT_STR_EQ("<t:123:R>", dc_string_cstr(&out), "timestamp style value");

    TEST_ASSERT_EQ(DC_OK, dc_format_timestamp_ms(123456, '\0', &out), "timestamp ms");
    TEST_ASSERT_STR_EQ("<t:123>", dc_string_cstr(&out), "timestamp ms value");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_format_timestamp(123, 'x', &out), "timestamp invalid style");

    TEST_ASSERT_EQ(DC_OK, dc_format_mention_emoji("smile", 555, 0, &out), "emoji mention");
    TEST_ASSERT_STR_EQ("<:smile:555>", dc_string_cstr(&out), "emoji mention value");
    TEST_ASSERT_EQ(DC_OK, dc_format_mention_emoji("wave", 777, 1, &out), "emoji mention animated");
    TEST_ASSERT_STR_EQ("<a:wave:777>", dc_string_cstr(&out), "emoji mention animated value");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_format_mention_emoji("bad:name", 777, 0, &out),
                   "emoji mention invalid name");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_format_mention_emoji("smile", 0, 0, &out),
                   "emoji mention invalid id");

    TEST_ASSERT_EQ(DC_OK,
                   dc_format_escape_content("Hello @everyone <#123> **bold**", &out),
                   "escape content");
    TEST_ASSERT_STR_EQ("Hello \\@everyone \\<\\#123\\> \\*\\*bold\\*\\*",
                       dc_string_cstr(&out),
                       "escape content value");
    TEST_ASSERT_EQ(DC_OK, dc_format_escape_content("back\\slash", &out), "escape backslash");
    TEST_ASSERT_STR_EQ("back\\\\slash", dc_string_cstr(&out), "escape backslash value");

    TEST_ASSERT_EQ(DC_OK, dc_format_escape_content("safe", &out), "escape safe");
    TEST_ASSERT_STR_EQ("safe", dc_string_cstr(&out), "escape safe value");

    dc_string_free(&out);

    TEST_SUITE_END("Format Tests");
}
