/**
 * @file dc_allowed_mentions.c
 * @brief Allowed mentions helper
 */

#include "dc_allowed_mentions.h"
#include <string.h>

dc_status_t dc_allowed_mentions_init(dc_allowed_mentions_t* mentions) {
    if (!mentions) return DC_ERROR_NULL_POINTER;
    memset(mentions, 0, sizeof(*mentions));
    dc_status_t st = dc_vec_init(&mentions->users, sizeof(dc_snowflake_t));
    if (st != DC_OK) return st;
    st = dc_vec_init(&mentions->roles, sizeof(dc_snowflake_t));
    if (st != DC_OK) {
        dc_vec_free(&mentions->users);
        return st;
    }
    return DC_OK;
}

void dc_allowed_mentions_free(dc_allowed_mentions_t* mentions) {
    if (!mentions) return;
    dc_vec_free(&mentions->users);
    dc_vec_free(&mentions->roles);
    memset(mentions, 0, sizeof(*mentions));
}

void dc_allowed_mentions_set_parse(dc_allowed_mentions_t* mentions,
                                   int users, int roles, int everyone) {
    if (!mentions) return;
    mentions->parse_set = 1;
    mentions->parse_users = users ? 1 : 0;
    mentions->parse_roles = roles ? 1 : 0;
    mentions->parse_everyone = everyone ? 1 : 0;
}

void dc_allowed_mentions_set_replied_user(dc_allowed_mentions_t* mentions, int replied_user) {
    if (!mentions) return;
    mentions->replied_user_set = 1;
    mentions->replied_user = replied_user ? 1 : 0;
}

dc_status_t dc_allowed_mentions_add_user(dc_allowed_mentions_t* mentions, dc_snowflake_t user_id) {
    if (!mentions) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(user_id)) return DC_ERROR_INVALID_PARAM;
    return dc_vec_push(&mentions->users, &user_id);
}

dc_status_t dc_allowed_mentions_add_role(dc_allowed_mentions_t* mentions, dc_snowflake_t role_id) {
    if (!mentions) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(role_id)) return DC_ERROR_INVALID_PARAM;
    return dc_vec_push(&mentions->roles, &role_id);
}
