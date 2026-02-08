#ifndef DC_GUILD_MEMBER_H
#define DC_GUILD_MEMBER_H

/**
 * @file dc_guild_member.h
 * @brief Discord Guild Member model
 */

#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_snowflake.h"
#include "core/dc_vec.h"
#include "model/dc_model_common.h"
#include "model/dc_user.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int has_user;
    dc_user_t user;
    dc_nullable_string_t nick;
    dc_nullable_string_t avatar;
    dc_vec_t roles; /* dc_snowflake_t */
    dc_string_t joined_at;
    dc_nullable_string_t premium_since;
    int deaf;
    int mute;
    dc_optional_bool_t pending;
    dc_optional_u64_field_t permissions;
    dc_nullable_string_t communication_disabled_until;
    uint32_t flags;
} dc_guild_member_t;

typedef struct {
    dc_vec_t items; /* dc_guild_member_t */
} dc_guild_member_list_t;

dc_status_t dc_guild_member_init(dc_guild_member_t* member);
void dc_guild_member_free(dc_guild_member_t* member);
dc_status_t dc_guild_member_from_json(const char* json_data, dc_guild_member_t* member);
dc_status_t dc_guild_member_to_json(const dc_guild_member_t* member, dc_string_t* json_result);

dc_status_t dc_guild_member_list_init(dc_guild_member_list_t* list);
void dc_guild_member_list_free(dc_guild_member_list_t* list);

#ifdef __cplusplus
}
#endif

#endif /* DC_GUILD_MEMBER_H */
