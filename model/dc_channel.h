#ifndef DC_CHANNEL_H
#define DC_CHANNEL_H

/**
 * @file dc_channel.h
 * @brief Discord Channel model
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

typedef enum {
    DC_CHANNEL_TYPE_GUILD_TEXT = 0,
    DC_CHANNEL_TYPE_DM = 1,
    DC_CHANNEL_TYPE_GUILD_VOICE = 2,
    DC_CHANNEL_TYPE_GROUP_DM = 3,
    DC_CHANNEL_TYPE_GUILD_CATEGORY = 4,
    DC_CHANNEL_TYPE_GUILD_ANNOUNCEMENT = 5,
    DC_CHANNEL_TYPE_ANNOUNCEMENT_THREAD = 10,
    DC_CHANNEL_TYPE_PUBLIC_THREAD = 11,
    DC_CHANNEL_TYPE_PRIVATE_THREAD = 12,
    DC_CHANNEL_TYPE_GUILD_STAGE_VOICE = 13,
    DC_CHANNEL_TYPE_GUILD_DIRECTORY = 14,
    DC_CHANNEL_TYPE_GUILD_FORUM = 15,
    DC_CHANNEL_TYPE_GUILD_MEDIA = 16
} dc_channel_type_t;

typedef enum {
    DC_PERMISSION_OVERWRITE_TYPE_ROLE = 0,
    DC_PERMISSION_OVERWRITE_TYPE_MEMBER = 1
} dc_permission_overwrite_type_t;

typedef struct {
    dc_snowflake_t id;
    dc_permission_overwrite_type_t type; /* 0 = role, 1 = member */
    uint64_t allow;
    uint64_t deny;
} dc_permission_overwrite_t;

typedef struct {
    int archived;
    int auto_archive_duration;
    dc_string_t archive_timestamp;
    int locked;
    DC_OPTIONAL(int) invitable;
    dc_nullable_string_t create_timestamp;
} dc_channel_thread_metadata_t;

typedef struct {
    dc_optional_snowflake_t id;
    dc_optional_snowflake_t user_id;
    dc_string_t join_timestamp;
    uint32_t flags;
} dc_channel_thread_member_t;

dc_status_t dc_channel_thread_member_init(dc_channel_thread_member_t* member);
void dc_channel_thread_member_free(dc_channel_thread_member_t* member);

typedef struct {
    dc_snowflake_t id;
    dc_string_t name;
    int moderated;
    dc_optional_snowflake_t emoji_id;
    dc_optional_string_t emoji_name;
} dc_channel_forum_tag_t;

typedef struct {
    dc_optional_snowflake_t emoji_id;
    dc_optional_string_t emoji_name;
} dc_channel_default_reaction_t;

typedef struct {
    dc_snowflake_t id;
    dc_channel_type_t type;
    dc_optional_snowflake_t guild_id;
    dc_optional_snowflake_t parent_id;
    dc_optional_snowflake_t last_message_id;
    dc_optional_snowflake_t owner_id;
    dc_optional_snowflake_t application_id;
    dc_string_t name;
    dc_string_t topic;
    dc_string_t icon;
    dc_string_t last_pin_timestamp;
    dc_string_t rtc_region;
    int position;
    dc_vec_t permission_overwrites; /* dc_permission_overwrite_t */
    int nsfw;
    int bitrate;
    int user_limit;
    int rate_limit_per_user;
    int default_auto_archive_duration;
    int default_thread_rate_limit_per_user;
    int video_quality_mode;
    int message_count;
    int member_count;
    uint64_t flags;
    dc_optional_u64_field_t permissions;
    int total_message_sent;
    int has_thread_metadata;
    dc_channel_thread_metadata_t thread_metadata;
    int has_thread_member;
    dc_channel_thread_member_t thread_member;
    dc_vec_t available_tags; /* dc_channel_forum_tag_t */
    dc_vec_t applied_tags;   /* dc_snowflake_t */
    int has_default_reaction_emoji;
    dc_channel_default_reaction_t default_reaction_emoji;
    int default_sort_order;
    int default_forum_layout;
} dc_channel_t;

typedef struct {
    dc_vec_t items; /* dc_channel_t */
} dc_channel_list_t;

dc_status_t dc_channel_init(dc_channel_t* channel);
void dc_channel_free(dc_channel_t* channel);
dc_status_t dc_channel_from_json(const char* json_data, dc_channel_t* channel);
dc_status_t dc_channel_to_json(const dc_channel_t* channel, dc_string_t* json_result);
dc_status_t dc_channel_list_init(dc_channel_list_t* list);
void dc_channel_list_free(dc_channel_list_t* list);

#ifdef __cplusplus
}
#endif

#endif /* DC_CHANNEL_H */
