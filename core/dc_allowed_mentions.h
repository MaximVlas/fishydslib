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

/**
 * @brief Allowed mentions payload controls for message create/edit.
 */
typedef struct {
    int parse_users;      /**< Include "users" in parse list */
    int parse_roles;      /**< Include "roles" in parse list */
    int parse_everyone;   /**< Include "everyone" in parse list */
    int parse_set;        /**< Whether parse_* fields were explicitly set */
    int replied_user;     /**< Whether to mention replied user */
    int replied_user_set; /**< Whether replied_user was explicitly set */
    dc_vec_t users;       /**< Explicit user mentions (dc_snowflake_t elements) */
    dc_vec_t roles;       /**< Explicit role mentions (dc_snowflake_t elements) */
} dc_allowed_mentions_t;

/**
 * @brief Initialize an allowed mentions structure.
 * @param mentions Structure to initialize.
 * @return DC_OK on success, error code on failure.
 */
dc_status_t dc_allowed_mentions_init(dc_allowed_mentions_t* mentions);

/**
 * @brief Free vectors and clear an allowed mentions structure.
 * @param mentions Structure to release (may be NULL).
 */
void dc_allowed_mentions_free(dc_allowed_mentions_t* mentions);

/**
 * @brief Configure parse behavior for users/roles/everyone.
 * @param mentions Target structure.
 * @param users Non-zero to parse user mentions automatically.
 * @param roles Non-zero to parse role mentions automatically.
 * @param everyone Non-zero to parse @everyone/@here automatically.
 */
void dc_allowed_mentions_set_parse(dc_allowed_mentions_t* mentions,
                                   int users, int roles, int everyone);

/**
 * @brief Configure whether reply author should be mentioned.
 * @param mentions Target structure.
 * @param replied_user Non-zero to mention the replied user.
 */
void dc_allowed_mentions_set_replied_user(dc_allowed_mentions_t* mentions, int replied_user);

/**
 * @brief Add an explicit user ID to allowed mentions.
 * @param mentions Target structure.
 * @param user_id User snowflake to allow.
 * @return DC_OK on success, error code on failure.
 */
dc_status_t dc_allowed_mentions_add_user(dc_allowed_mentions_t* mentions, dc_snowflake_t user_id);

/**
 * @brief Add an explicit role ID to allowed mentions.
 * @param mentions Target structure.
 * @param role_id Role snowflake to allow.
 * @return DC_OK on success, error code on failure.
 */
dc_status_t dc_allowed_mentions_add_role(dc_allowed_mentions_t* mentions, dc_snowflake_t role_id);

#ifdef __cplusplus
}
#endif

#endif /* DC_ALLOWED_MENTIONS_H */
