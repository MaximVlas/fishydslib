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
#include "model/dc_attachment.h"
#include "model/dc_embed.h"
#include "model/dc_guild_member.h"

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
    DC_MESSAGE_TYPE_STAGE_TOPIC = 31,
    DC_MESSAGE_TYPE_GUILD_APPLICATION_PREMIUM_SUBSCRIPTION = 32,
    DC_MESSAGE_TYPE_GUILD_INCIDENT_ALERT_MODE_ENABLED = 36,
    DC_MESSAGE_TYPE_GUILD_INCIDENT_ALERT_MODE_DISABLED = 37,
    DC_MESSAGE_TYPE_GUILD_INCIDENT_REPORT_RAID = 38,
    DC_MESSAGE_TYPE_GUILD_INCIDENT_REPORT_FALSE_ALARM = 39,
    DC_MESSAGE_TYPE_PURCHASE_NOTIFICATION = 44,
    DC_MESSAGE_TYPE_POLL_RESULT = 46
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

/**
 * @brief Message reference types
 */
typedef enum {
    DC_MESSAGE_REFERENCE_TYPE_DEFAULT = 0,
    DC_MESSAGE_REFERENCE_TYPE_FORWARD = 1
} dc_message_reference_type_t;

/**
 * @brief Message activity types
 */
typedef enum {
    DC_MESSAGE_ACTIVITY_TYPE_JOIN = 1,
    DC_MESSAGE_ACTIVITY_TYPE_SPECTATE = 2,
    DC_MESSAGE_ACTIVITY_TYPE_LISTEN = 3,
    DC_MESSAGE_ACTIVITY_TYPE_JOIN_REQUEST = 5
} dc_message_activity_type_t;

/**
 * @brief Sticker format types
 */
typedef enum {
    DC_STICKER_FORMAT_PNG = 1,
    DC_STICKER_FORMAT_APNG = 2,
    DC_STICKER_FORMAT_LOTTIE = 3,
    DC_STICKER_FORMAT_GIF = 4
} dc_sticker_format_type_t;

/**
 * @brief Message reference object
 * @see https://discord.com/developers/docs/resources/message#message-reference-structure
 */
typedef struct {
    dc_message_reference_type_t type;
    dc_optional_snowflake_t message_id;
    dc_optional_snowflake_t channel_id;
    dc_optional_snowflake_t guild_id;
    int fail_if_not_exists; /**< Default true */
} dc_message_reference_t;

/**
 * @brief Reaction count details
 */
typedef struct {
    int burst;
    int normal;
} dc_reaction_count_details_t;

/**
 * @brief Reaction object
 * @see https://discord.com/developers/docs/resources/message#reaction-object
 */
typedef struct {
    int count;
    dc_reaction_count_details_t count_details;
    int me;
    int me_burst;
    dc_optional_snowflake_t emoji_id;
    dc_string_t emoji_name;
    dc_vec_t burst_colors; /**< dc_string_t — HEX color strings */
} dc_reaction_t;

/**
 * @brief Sticker item object
 * @see https://discord.com/developers/docs/resources/sticker#sticker-item-object
 */
typedef struct {
    dc_snowflake_t id;
    dc_string_t name;
    dc_sticker_format_type_t format_type;
} dc_sticker_item_t;

/**
 * @brief Channel mention object (cross-posted messages)
 * @see https://discord.com/developers/docs/resources/message#channel-mention-object
 */
typedef struct {
    dc_snowflake_t id;
    dc_snowflake_t guild_id;
    int type;
    dc_string_t name;
} dc_channel_mention_t;

/**
 * @brief Role subscription data
 * @see https://discord.com/developers/docs/resources/message#role-subscription-data-object
 */
typedef struct {
    dc_snowflake_t role_subscription_listing_id;
    dc_string_t tier_name;
    int total_months_subscribed;
    int is_renewal;
} dc_role_subscription_data_t;

/**
 * @brief Message call object
 * @see https://discord.com/developers/docs/resources/message#message-call-object
 */
typedef struct {
    dc_vec_t participants;       /**< dc_snowflake_t */
    dc_nullable_string_t ended_timestamp;
} dc_message_call_t;

/**
 * @brief Message activity object
 * @see https://discord.com/developers/docs/resources/message#message-activity-object
 */
typedef struct {
    dc_message_activity_type_t type;
    dc_optional_string_t party_id;
} dc_message_activity_t;

typedef struct dc_message dc_message_t;

struct dc_message {
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
    int has_application;
    dc_string_t application_json; /* partial application object (raw JSON) */
    int has_thread;
    dc_channel_t thread;
    dc_vec_t components; /* dc_component_t */
    dc_vec_t attachments; /* dc_attachment_t */
    dc_vec_t embeds;      /* dc_embed_t */
    dc_vec_t mentions;    /* dc_guild_member_t (using guild member to capture user + partial member) */

    /* message_reference */
    int has_message_reference;
    dc_message_reference_t message_reference;

    /* referenced_message (nullable, heap-allocated to avoid recursive struct) */
    dc_message_t* referenced_message;
    int has_message_snapshots;
    dc_string_t message_snapshots_json; /* array of message snapshot objects (raw JSON) */
    int has_interaction_metadata;
    dc_string_t interaction_metadata_json; /* message interaction metadata object (raw JSON) */

    /* nonce */
    dc_optional_string_t nonce;

    /* reactions */
    dc_vec_t reactions;         /* dc_reaction_t */

    /* sticker_items */
    dc_vec_t sticker_items;     /* dc_sticker_item_t */

    /* mention_channels */
    dc_vec_t mention_channels;  /* dc_channel_mention_t */

    /* position (approximate position in thread) */
    dc_optional_i32_t position;

    /* role_subscription_data */
    int has_role_subscription_data;
    dc_role_subscription_data_t role_subscription_data;
    int has_resolved;
    dc_string_t resolved_json; /* interaction-style resolved data (raw JSON) */
    int has_poll;
    dc_string_t poll_json; /* poll object (raw JSON) */

    /* call */
    int has_call;
    dc_message_call_t call;

    /* activity */
    int has_activity;
    dc_message_activity_t activity;
};

dc_status_t dc_message_init(dc_message_t* message);
void dc_message_free(dc_message_t* message);
dc_status_t dc_message_from_json(const char* json_data, dc_message_t* message);
dc_status_t dc_message_to_json(const dc_message_t* message, dc_string_t* json_result);

dc_status_t dc_reaction_init(dc_reaction_t* reaction);
void dc_reaction_free(dc_reaction_t* reaction);
dc_status_t dc_sticker_item_init(dc_sticker_item_t* item);
void dc_sticker_item_free(dc_sticker_item_t* item);
dc_status_t dc_channel_mention_init(dc_channel_mention_t* mention);
void dc_channel_mention_free(dc_channel_mention_t* mention);
dc_status_t dc_role_subscription_data_init(dc_role_subscription_data_t* data);
void dc_role_subscription_data_free(dc_role_subscription_data_t* data);
dc_status_t dc_message_call_init(dc_message_call_t* call);
void dc_message_call_free(dc_message_call_t* call);
dc_status_t dc_message_activity_init(dc_message_activity_t* activity);
void dc_message_activity_free(dc_message_activity_t* activity);
dc_status_t dc_message_reference_init(dc_message_reference_t* ref);
void dc_message_reference_free(dc_message_reference_t* ref);

#ifdef __cplusplus
}
#endif

#endif /* DC_MESSAGE_H */
