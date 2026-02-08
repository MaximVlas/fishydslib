/**
 * @file test_permissions.c
 * @brief Permissions computation tests
 */

#include "test_utils.h"
#include "model/dc_permissions.h"

static dc_status_t push_role(dc_role_list_t* roles, dc_snowflake_t id, dc_permissions_t perms) {
    dc_role_t role;
    dc_status_t st = dc_role_init(&role);
    if (st != DC_OK) return st;
    role.id = id;
    role.permissions = perms;
    return dc_vec_push(&roles->items, &role);
}

static dc_status_t push_member_role(dc_guild_member_t* member, dc_snowflake_t role_id) {
    return dc_vec_push(&member->roles, &role_id);
}

static dc_status_t push_overwrite(dc_channel_t* channel,
                                  dc_snowflake_t id,
                                  dc_permission_overwrite_type_t type,
                                  dc_permissions_t allow,
                                  dc_permissions_t deny) {
    dc_permission_overwrite_t ow;
    ow.id = id;
    ow.type = type;
    ow.allow = allow;
    ow.deny = deny;
    return dc_vec_push(&channel->permission_overwrites, &ow);
}

int test_permissions_main(void) {
    TEST_SUITE_BEGIN("Permissions Tests");

    const dc_snowflake_t guild_id = 100;
    const dc_snowflake_t owner_id = 999;
    const dc_snowflake_t member_id = 500;
    const dc_snowflake_t role_a = 200;
    const dc_snowflake_t role_b = 300;
    const dc_snowflake_t role_admin = 400;

    /* Base permissions: @everyone + member roles */
    {
        dc_role_list_t roles;
        TEST_ASSERT_EQ(DC_OK, dc_role_list_init(&roles), "roles init");
        TEST_ASSERT_EQ(DC_OK, push_role(&roles, guild_id, DC_PERMISSION_VIEW_CHANNEL), "@everyone role");
        TEST_ASSERT_EQ(DC_OK, push_role(&roles, role_a, DC_PERMISSION_SEND_MESSAGES | DC_PERMISSION_EMBED_LINKS),
                       "role A");
        TEST_ASSERT_EQ(DC_OK, push_role(&roles, role_b, DC_PERMISSION_ADD_REACTIONS), "role B");

        dc_guild_member_t member;
        TEST_ASSERT_EQ(DC_OK, dc_guild_member_init(&member), "member init");
        member.has_user = 1;
        member.user.id = member_id;
        TEST_ASSERT_EQ(DC_OK, push_member_role(&member, role_a), "member role A");
        TEST_ASSERT_EQ(DC_OK, push_member_role(&member, role_b), "member role B");

        dc_permissions_t base = 0;
        TEST_ASSERT_EQ(
            DC_OK,
            dc_permissions_compute_base(guild_id, owner_id, member.user.id, &roles, &member.roles, &base),
            "compute base permissions");
        dc_permissions_t expected = DC_PERMISSION_VIEW_CHANNEL | DC_PERMISSION_SEND_MESSAGES |
                                    DC_PERMISSION_EMBED_LINKS | DC_PERMISSION_ADD_REACTIONS;
        TEST_ASSERT_EQ(expected, base, "base permissions OR");

        dc_guild_member_free(&member);
        dc_role_list_free(&roles);
    }

    /* Owner => ALL */
    {
        dc_role_list_t roles;
        TEST_ASSERT_EQ(DC_OK, dc_role_list_init(&roles), "roles init (owner)");
        TEST_ASSERT_EQ(DC_OK, push_role(&roles, guild_id, DC_PERMISSION_VIEW_CHANNEL), "@everyone role (owner)");

        dc_permissions_t base = 0;
        TEST_ASSERT_EQ(DC_OK, dc_permissions_compute_base(guild_id, member_id, member_id, &roles, NULL, &base),
                       "compute base (owner)");
        TEST_ASSERT_EQ(DC_PERMISSIONS_ALL, base, "owner gets ALL");
        dc_role_list_free(&roles);
    }

    /* ADMINISTRATOR => ALL */
    {
        dc_role_list_t roles;
        TEST_ASSERT_EQ(DC_OK, dc_role_list_init(&roles), "roles init (admin)");
        TEST_ASSERT_EQ(DC_OK, push_role(&roles, guild_id, DC_PERMISSIONS_NONE), "@everyone role (admin)");
        TEST_ASSERT_EQ(DC_OK, push_role(&roles, role_admin, DC_PERMISSION_ADMINISTRATOR), "admin role");

        dc_guild_member_t member;
        TEST_ASSERT_EQ(DC_OK, dc_guild_member_init(&member), "member init (admin)");
        member.has_user = 1;
        member.user.id = member_id;
        TEST_ASSERT_EQ(DC_OK, push_member_role(&member, role_admin), "member has admin role");

        dc_permissions_t base = 0;
        TEST_ASSERT_EQ(
            DC_OK,
            dc_permissions_compute_base(guild_id, owner_id, member.user.id, &roles, &member.roles, &base),
            "compute base (admin)");
        TEST_ASSERT_EQ(DC_PERMISSIONS_ALL, base, "admin gets ALL");

        dc_guild_member_free(&member);
        dc_role_list_free(&roles);
    }

    /* Overwrites: everyone, roles, member */
    {
        dc_role_list_t roles;
        TEST_ASSERT_EQ(DC_OK, dc_role_list_init(&roles), "roles init (overwrites)");
        TEST_ASSERT_EQ(DC_OK, push_role(&roles, guild_id, DC_PERMISSION_VIEW_CHANNEL), "@everyone role (overwrites)");
        TEST_ASSERT_EQ(DC_OK, push_role(&roles, role_a, DC_PERMISSION_SEND_MESSAGES | DC_PERMISSION_EMBED_LINKS),
                       "role A (overwrites)");
        TEST_ASSERT_EQ(DC_OK, push_role(&roles, role_b, DC_PERMISSION_ADD_REACTIONS), "role B (overwrites)");

        dc_guild_member_t member;
        TEST_ASSERT_EQ(DC_OK, dc_guild_member_init(&member), "member init (overwrites)");
        member.has_user = 1;
        member.user.id = member_id;
        TEST_ASSERT_EQ(DC_OK, push_member_role(&member, role_a), "member role A (overwrites)");
        TEST_ASSERT_EQ(DC_OK, push_member_role(&member, role_b), "member role B (overwrites)");

        dc_channel_t channel;
        TEST_ASSERT_EQ(DC_OK, dc_channel_init(&channel), "channel init (overwrites)");
        channel.id = 42;
        channel.type = DC_CHANNEL_TYPE_GUILD_TEXT;

        /* Everyone overwrite: deny SEND_MESSAGES, allow ADD_REACTIONS */
        TEST_ASSERT_EQ(DC_OK,
                       push_overwrite(&channel, guild_id, DC_PERMISSION_OVERWRITE_TYPE_ROLE,
                                      DC_PERMISSION_ADD_REACTIONS, DC_PERMISSION_SEND_MESSAGES),
                       "push everyone overwrite");
        /* Role overwrites */
        TEST_ASSERT_EQ(DC_OK,
                       push_overwrite(&channel, role_a, DC_PERMISSION_OVERWRITE_TYPE_ROLE,
                                      DC_PERMISSION_SEND_MESSAGES, DC_PERMISSION_EMBED_LINKS),
                       "push role A overwrite");
        TEST_ASSERT_EQ(DC_OK,
                       push_overwrite(&channel, role_b, DC_PERMISSION_OVERWRITE_TYPE_ROLE,
                                      DC_PERMISSIONS_NONE, DC_PERMISSION_ADD_REACTIONS),
                       "push role B overwrite");
        /* Member overwrite: re-allow EMBED_LINKS */
        TEST_ASSERT_EQ(DC_OK,
                       push_overwrite(&channel, member_id, DC_PERMISSION_OVERWRITE_TYPE_MEMBER,
                                      DC_PERMISSION_EMBED_LINKS, DC_PERMISSIONS_NONE),
                       "push member overwrite");

        dc_permissions_t computed = 0;
        TEST_ASSERT_EQ(
            DC_OK,
            dc_permissions_compute_channel(guild_id, owner_id, &roles, &member, &channel, &computed),
            "compute channel permissions");

        dc_permissions_t expected = DC_PERMISSION_VIEW_CHANNEL | DC_PERMISSION_SEND_MESSAGES | DC_PERMISSION_EMBED_LINKS;
        TEST_ASSERT_EQ(expected, computed, "overwrite resolution result");

        dc_channel_free(&channel);
        dc_guild_member_free(&member);
        dc_role_list_free(&roles);
    }

    /* Implicit permissions (text) */
    {
        dc_permissions_t perms = DC_PERMISSION_SEND_MESSAGES;
        TEST_ASSERT_EQ(DC_PERMISSIONS_NONE, dc_permissions_apply_implicit_text(perms), "implicit: no VIEW_CHANNEL");

        perms = DC_PERMISSION_VIEW_CHANNEL | DC_PERMISSION_MENTION_EVERYONE | DC_PERMISSION_ATTACH_FILES |
                DC_PERMISSION_EMBED_LINKS | DC_PERMISSION_ADD_REACTIONS;
        dc_permissions_t out = dc_permissions_apply_implicit_text(perms);
        dc_permissions_t expected = DC_PERMISSION_VIEW_CHANNEL | DC_PERMISSION_ADD_REACTIONS;
        TEST_ASSERT_EQ(expected, out, "implicit: no SEND_MESSAGES clears dependent bits");
    }

    /* Thread rules */
    {
        dc_permissions_t perms = DC_PERMISSION_VIEW_CHANNEL | DC_PERMISSION_SEND_MESSAGES |
                                 DC_PERMISSION_SEND_MESSAGES_IN_THREADS;
        dc_permissions_t out = dc_permissions_apply_thread_rules(perms, DC_CHANNEL_TYPE_PUBLIC_THREAD);
        dc_permissions_t expected = DC_PERMISSION_VIEW_CHANNEL | DC_PERMISSION_SEND_MESSAGES_IN_THREADS;
        TEST_ASSERT_EQ(expected, out, "thread rules clear SEND_MESSAGES");
    }

    /* Timed out mask */
    {
        dc_permissions_t perms = DC_PERMISSION_VIEW_CHANNEL | DC_PERMISSION_READ_MESSAGE_HISTORY |
                                 DC_PERMISSION_SEND_MESSAGES | DC_PERMISSION_EMBED_LINKS;
        dc_permissions_t out = dc_permissions_apply_timed_out_mask(perms);
        dc_permissions_t expected = DC_PERMISSION_VIEW_CHANNEL | DC_PERMISSION_READ_MESSAGE_HISTORY;
        TEST_ASSERT_EQ(expected, out, "timed out mask keeps only VIEW_CHANNEL + READ_MESSAGE_HISTORY");
    }

    TEST_SUITE_END("Permissions Tests");
}

