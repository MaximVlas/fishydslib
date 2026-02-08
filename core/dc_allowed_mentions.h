#ifndef DC_ALLOWED_MENTIONS_H
#define DC_ALLOWED_MENTIONS_H

/**
 * @file dc_allowed_mentions.h
 * @brief Allowed mentions helper
 */

#include "dc_status.h"
#include "dc_vec.h"
#include "dc_snowflake.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int parse_users;
    int parse_roles;
    int parse_everyone;
    int parse_set;
    int replied_user;
    int replied_user_set;
    dc_vec_t users; /* dc_snowflake_t */
    dc_vec_t roles; /* dc_snowflake_t */
} dc_allowed_mentions_t;

dc_status_t dc_allowed_mentions_init(dc_allowed_mentions_t* mentions);
void dc_allowed_mentions_free(dc_allowed_mentions_t* mentions);

void dc_allowed_mentions_set_parse(dc_allowed_mentions_t* mentions,
                                   int users, int roles, int everyone);
void dc_allowed_mentions_set_replied_user(dc_allowed_mentions_t* mentions, int replied_user);

dc_status_t dc_allowed_mentions_add_user(dc_allowed_mentions_t* mentions, dc_snowflake_t user_id);
dc_status_t dc_allowed_mentions_add_role(dc_allowed_mentions_t* mentions, dc_snowflake_t role_id);

#ifdef __cplusplus
}
#endif

#endif /* DC_ALLOWED_MENTIONS_H */
