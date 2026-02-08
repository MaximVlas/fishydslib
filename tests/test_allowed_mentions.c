/**
 * @file test_allowed_mentions.c
 * @brief Allowed mentions tests
 */

#include "test_utils.h"
#include "core/dc_allowed_mentions.h"

int test_allowed_mentions_main(void) {
    TEST_SUITE_BEGIN("Allowed Mentions Tests");

    dc_allowed_mentions_t mentions;
    TEST_ASSERT_EQ(DC_OK, dc_allowed_mentions_init(&mentions), "allowed mentions init");

    dc_allowed_mentions_set_parse(&mentions, 1, 0, 1);
    dc_allowed_mentions_set_replied_user(&mentions, 1);
    TEST_ASSERT_EQ(1, mentions.parse_set, "allowed mentions parse set");
    TEST_ASSERT_EQ(1, mentions.parse_users, "allowed mentions parse users");
    TEST_ASSERT_EQ(0, mentions.parse_roles, "allowed mentions parse roles");
    TEST_ASSERT_EQ(1, mentions.parse_everyone, "allowed mentions parse everyone");
    TEST_ASSERT_EQ(1, mentions.replied_user_set, "allowed mentions replied set");
    TEST_ASSERT_EQ(1, mentions.replied_user, "allowed mentions replied value");

    TEST_ASSERT_EQ(DC_OK, dc_allowed_mentions_add_user(&mentions, 123), "allowed mentions add user");
    TEST_ASSERT_EQ(DC_OK, dc_allowed_mentions_add_role(&mentions, 456), "allowed mentions add role");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_allowed_mentions_add_user(&mentions, 0),
                   "allowed mentions add user invalid");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_allowed_mentions_add_role(&mentions, 0),
                   "allowed mentions add role invalid");

    dc_allowed_mentions_free(&mentions);

    TEST_SUITE_END("Allowed Mentions Tests");
}
