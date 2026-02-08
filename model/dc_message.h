#ifndef DC_MESSAGE_H
#define DC_MESSAGE_H

/**
 * @file dc_message.h
 * @brief Discord Message model
 */

#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_snowflake.h"
#include "core/dc_vec.h"
#include "model/dc_model_common.h"
#include "model/dc_user.h"
#include "model/dc_channel.h"
#include "model/dc_component.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DC_MESSAGE_TYPE_DEFAULT = 0,
    DC_MESSAGE_TYPE_RECIPIENT_ADD = 1,
    DC_MESSAGE_TYPE_RECIPIENT_REMOVE = 2,
    DC_MESSAGE_TYPE_CALL = 3,
    DC_MESSAGE_TYPE_CHANNEL_NAME_CHANGE = 4,
    DC_MESSAGE_TYPE_CHANNEL_ICON_CHANGE = 5,
    DC_MESSAGE_TYPE_CHANNEL_PINNED_MESSAGE = 6,
    DC_MESSAGE_TYPE_USER_JOIN = 7,
    DC_MESSAGE_TYPE_GUILD_BOOST = 8,
    DC_MESSAGE_TYPE_GUILD_BOOST_TIER_1 = 9,
    DC_MESSAGE_TYPE_GUILD_BOOST_TIER_2 = 10,
    DC_MESSAGE_TYPE_GUILD_BOOST_TIER_3 = 11,
    DC_MESSAGE_TYPE_CHANNEL_FOLLOW_ADD = 12,
    DC_MESSAGE_TYPE_GUILD_DISCOVERY_DISQUALIFIED = 14,
    DC_MESSAGE_TYPE_GUILD_DISCOVERY_REQUALIFIED = 15,
    DC_MESSAGE_TYPE_GUILD_DISCOVERY_GRACE_PERIOD_INITIAL_WARNING = 16,
    DC_MESSAGE_TYPE_GUILD_DISCOVERY_GRACE_PERIOD_FINAL_WARNING = 17,
    DC_MESSAGE_TYPE_THREAD_CREATED = 18,
    DC_MESSAGE_TYPE_REPLY = 19,
    DC_MESSAGE_TYPE_CHAT_INPUT_COMMAND = 20,
    DC_MESSAGE_TYPE_THREAD_STARTER_MESSAGE = 21,
    DC_MESSAGE_TYPE_GUILD_INVITE_REMINDER = 22,
    DC_MESSAGE_TYPE_CONTEXT_MENU_COMMAND = 23,
    DC_MESSAGE_TYPE_AUTO_MODERATION_ACTION = 24,
    DC_MESSAGE_TYPE_ROLE_SUBSCRIPTION_PURCHASE = 25,
    DC_MESSAGE_TYPE_INTERACTION_PREMIUM_UPSELL = 26,
    DC_MESSAGE_TYPE_STAGE_START = 27,
    DC_MESSAGE_TYPE_STAGE_END = 28,
    DC_MESSAGE_TYPE_STAGE_SPEAKER = 29,
    DC_MESSAGE_TYPE_STAGE_TOPIC = 30,
    DC_MESSAGE_TYPE_GUILD_APPLICATION_PREMIUM_SUBSCRIPTION = 31,
    DC_MESSAGE_TYPE_GUILD_INCIDENT_ALERT_MODE_ENABLED = 32,
    DC_MESSAGE_TYPE_GUILD_INCIDENT_ALERT_MODE_DISABLED = 33,
    DC_MESSAGE_TYPE_GUILD_INCIDENT_REPORT_RAID = 34,
    DC_MESSAGE_TYPE_GUILD_INCIDENT_REPORT_FALSE_ALARM = 35,
    DC_MESSAGE_TYPE_PURCHASE_NOTIFICATION = 36
} dc_message_type_t;

typedef enum {
    DC_MESSAGE_FLAG_CROSSPOSTED = 1ull << 0,
    DC_MESSAGE_FLAG_IS_CROSSPOST = 1ull << 1,
    DC_MESSAGE_FLAG_SUPPRESS_EMBEDS = 1ull << 2,
    DC_MESSAGE_FLAG_SOURCE_MESSAGE_DELETED = 1ull << 3,
    DC_MESSAGE_FLAG_URGENT = 1ull << 4,
    DC_MESSAGE_FLAG_HAS_THREAD = 1ull << 5,
    DC_MESSAGE_FLAG_EPHEMERAL = 1ull << 6,
    DC_MESSAGE_FLAG_LOADING = 1ull << 7,
    DC_MESSAGE_FLAG_FAILED_TO_MENTION_SOME_ROLES_IN_THREAD = 1ull << 8,
    DC_MESSAGE_FLAG_SUPPRESS_NOTIFICATIONS = 1ull << 12,
    DC_MESSAGE_FLAG_IS_VOICE_MESSAGE = 1ull << 13,
    DC_MESSAGE_FLAG_HAS_SNAPSHOT = 1ull << 14,
    DC_MESSAGE_FLAG_IS_COMPONENTS_V2 = 1ull << 15
} dc_message_flag_t;

typedef struct {
    dc_snowflake_t id;
    dc_snowflake_t channel_id;
    dc_user_t author;
    dc_string_t content;
    dc_string_t timestamp;
    dc_nullable_string_t edited_timestamp;
    int tts;
    int mention_everyone;
    int pinned;
    dc_message_type_t type;
    uint64_t flags;
    dc_optional_snowflake_t webhook_id;
    dc_optional_snowflake_t application_id;
    dc_vec_t mention_roles; /* dc_snowflake_t */
    int has_thread;
    dc_channel_t thread;
    dc_vec_t components; /* dc_component_t */
} dc_message_t;

dc_status_t dc_message_init(dc_message_t* message);
void dc_message_free(dc_message_t* message);
dc_status_t dc_message_from_json(const char* json_data, dc_message_t* message);
dc_status_t dc_message_to_json(const dc_message_t* message, dc_string_t* json_result);

#ifdef __cplusplus
}
#endif

#endif /* DC_MESSAGE_H */
