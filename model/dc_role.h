#ifndef DC_ROLE_H
#define DC_ROLE_H

/**
 * @file dc_role.h
 * @brief Discord Role model
 */

#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_snowflake.h"
#include "core/dc_vec.h"
#include "model/dc_model_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dc_optional_snowflake_t bot_id;
    dc_optional_snowflake_t integration_id;
    dc_optional_snowflake_t subscription_listing_id;
    dc_optional_bool_t premium_subscriber;
    dc_optional_bool_t available_for_purchase;
    dc_optional_bool_t guild_connections;
} dc_role_tags_t;

typedef struct {
    dc_snowflake_t id;
    dc_string_t name;
    uint32_t color;
    int hoist;
    dc_nullable_string_t icon;
    dc_nullable_string_t unicode_emoji;
    int position;
    uint64_t permissions;
    int managed;
    int mentionable;
    uint64_t flags;
    dc_role_tags_t tags;
} dc_role_t;

typedef struct {
    dc_vec_t items; /* dc_role_t */
} dc_role_list_t;

dc_status_t dc_role_init(dc_role_t* role);
void dc_role_free(dc_role_t* role);
dc_status_t dc_role_from_json(const char* json_data, dc_role_t* role);
dc_status_t dc_role_to_json(const dc_role_t* role, dc_string_t* json_result);

dc_status_t dc_role_list_init(dc_role_list_t* list);
void dc_role_list_free(dc_role_list_t* list);

#ifdef __cplusplus
}
#endif

#endif /* DC_ROLE_H */
