#ifndef DC_GUILD_H
#define DC_GUILD_H

/**
 * @file dc_guild.h
 * @brief Discord Guild model
 */

#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_snowflake.h"
#include "core/dc_vec.h"
#include "model/dc_model_common.h"
#include "model/dc_role.h"
#include "model/dc_user.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Discord Guild structure (v10 subset, safely extensible)
 */
typedef struct {
    dc_optional_snowflake_t id;                    /**< Emoji ID (may be null for unicode emoji) */
    dc_nullable_string_t name;                     /**< Emoji name */
    dc_vec_t roles;                                /**< Role IDs allowed to use this emoji (dc_snowflake_t) */
    int has_user;                                  /**< Whether user was present */
    dc_user_t user;                                /**< User who created this emoji */
    dc_optional_bool_t require_colons;             /**< Whether colons are required */
    dc_optional_bool_t managed;                    /**< Whether emoji is managed */
    dc_optional_bool_t animated;                   /**< Whether emoji is animated */
    dc_optional_bool_t available;                  /**< Whether emoji is available */
} dc_guild_emoji_t;

typedef struct {
    dc_vec_t items;                                /**< dc_guild_emoji_t */
} dc_guild_emoji_list_t;

typedef struct {
    dc_snowflake_t id;                             /**< Sticker ID */
    dc_optional_snowflake_t pack_id;               /**< Standard sticker pack ID */
    dc_string_t name;                              /**< Sticker name */
    dc_nullable_string_t description;              /**< Sticker description */
    dc_string_t tags;                              /**< Sticker tags */
    int type;                                      /**< Sticker type */
    int format_type;                               /**< Sticker format type */
    dc_optional_bool_t available;                  /**< Whether sticker is available */
    dc_optional_snowflake_t guild_id;              /**< Guild ID that owns the sticker */
    int has_user;                                  /**< Whether user was present */
    dc_user_t user;                                /**< User who uploaded this sticker */
    dc_optional_i32_t sort_value;                  /**< Sort value in guild list */
} dc_guild_sticker_t;

typedef struct {
    dc_vec_t items;                                /**< dc_guild_sticker_t */
} dc_guild_sticker_list_t;

typedef struct {
    dc_snowflake_t channel_id;                     /**< Welcome channel ID */
    dc_string_t description;                       /**< Welcome channel description */
    dc_nullable_snowflake_t emoji_id;              /**< Welcome channel emoji ID */
    dc_nullable_string_t emoji_name;               /**< Welcome channel unicode emoji */
} dc_guild_welcome_channel_t;

typedef struct {
    dc_nullable_string_t description;              /**< Welcome screen description */
    dc_vec_t welcome_channels;                     /**< dc_guild_welcome_channel_t */
} dc_guild_welcome_screen_t;

typedef struct {
    dc_nullable_string_t invites_disabled_until;   /**< Invites disabled until timestamp */
    dc_nullable_string_t dms_disabled_until;       /**< DMs disabled until timestamp */
    dc_nullable_string_t dm_spam_detected_at;      /**< DM spam detected timestamp */
    dc_nullable_string_t raid_detected_at;         /**< Raid detected timestamp */
} dc_guild_incidents_data_t;

typedef struct {
    dc_snowflake_t id;                             /**< Guild ID */
    dc_string_t name;                              /**< Guild name */
    dc_nullable_string_t icon;                     /**< Icon hash */
    dc_nullable_string_t icon_hash;                /**< Template icon hash */
    dc_nullable_string_t splash;                   /**< Splash hash */
    dc_nullable_string_t discovery_splash;         /**< Discovery splash hash */
    dc_optional_bool_t owner;                      /**< Whether current user is owner */
    dc_optional_snowflake_t owner_id;              /**< Owner user ID */
    dc_optional_u64_field_t permissions;           /**< Current user guild permissions */
    dc_optional_snowflake_t afk_channel_id;        /**< AFK channel ID */
    int afk_timeout;                               /**< AFK timeout seconds */
    dc_optional_bool_t widget_enabled;             /**< Widget enabled flag */
    dc_optional_snowflake_t widget_channel_id;     /**< Widget invite channel ID */
    int verification_level;                        /**< Verification level */
    int default_message_notifications;             /**< Default message notification level */
    int explicit_content_filter;                   /**< Explicit content filter level */
    int mfa_level;                                 /**< MFA requirement level */
    dc_optional_snowflake_t application_id;        /**< Bot-created guild application ID */
    dc_optional_snowflake_t system_channel_id;     /**< System channel ID */
    uint64_t system_channel_flags;                 /**< System channel flags */
    dc_optional_snowflake_t rules_channel_id;      /**< Rules channel ID */
    dc_optional_i32_t max_presences;               /**< Optional max presences */
    dc_optional_i32_t max_members;                 /**< Optional max members */
    dc_nullable_string_t vanity_url_code;          /**< Vanity URL code */
    dc_nullable_string_t description;              /**< Guild description */
    dc_nullable_string_t banner;                   /**< Banner hash */
    int has_features;                              /**< Whether features was present */
    dc_vec_t features;                             /**< Guild feature list (dc_string_t) */
    int has_roles;                                 /**< Whether roles was present */
    dc_role_list_t roles;                          /**< Parsed role objects */
    dc_string_t roles_json;                        /**< Raw roles array JSON */
    int has_emojis;                                /**< Whether emojis was present */
    dc_guild_emoji_list_t emojis;                  /**< Parsed emoji objects */
    dc_string_t emojis_json;                       /**< Raw emojis array JSON */
    int has_welcome_screen;                        /**< Whether welcome_screen was present */
    dc_guild_welcome_screen_t welcome_screen;      /**< Parsed welcome_screen object */
    dc_string_t welcome_screen_json;               /**< Raw welcome_screen object JSON */
    int has_stickers;                              /**< Whether stickers was present */
    dc_guild_sticker_list_t stickers;              /**< Parsed sticker objects */
    dc_string_t stickers_json;                     /**< Raw stickers array JSON */
    int has_incidents_data;                        /**< Whether incidents_data was present */
    dc_guild_incidents_data_t incidents_data;      /**< Parsed incidents_data object */
    dc_string_t incidents_data_json;               /**< Raw incidents_data object JSON */
    int premium_tier;                              /**< Boost tier */
    dc_optional_i32_t premium_subscription_count;  /**< Optional boost count */
    dc_string_t preferred_locale;                  /**< Preferred locale */
    dc_optional_snowflake_t public_updates_channel_id; /**< Public updates channel ID */
    dc_optional_i32_t max_video_channel_users;     /**< Optional max video users */
    dc_optional_i32_t max_stage_video_channel_users; /**< Optional max stage video users */
    dc_optional_i32_t approximate_member_count;    /**< Optional approximate member count */
    dc_optional_i32_t approximate_presence_count;  /**< Optional approximate presence count */
    int nsfw_level;                                /**< NSFW level */
    int premium_progress_bar_enabled;              /**< Boost progress bar enabled */
    dc_optional_snowflake_t safety_alerts_channel_id; /**< Safety alerts channel ID */
} dc_guild_t;

/**
 * @brief Initialize guild structure
 * @param guild Guild to initialize
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_guild_init(dc_guild_t* guild);

/**
 * @brief Free guild structure
 * @param guild Guild to free
 */
void dc_guild_free(dc_guild_t* guild);

/**
 * @brief Parse guild from JSON
 * @param json_data JSON data
 * @param guild Guild to store result
 * @return DC_OK on success, error code on failure
 *
 * @note Caller must free any prior guild data in @p guild before reuse.
 */
dc_status_t dc_guild_from_json(const char* json_data, dc_guild_t* guild);

/**
 * @brief Serialize guild to JSON
 * @param guild Guild to serialize
 * @param json_result String to store JSON result
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_guild_to_json(const dc_guild_t* guild, dc_string_t* json_result);

#ifdef __cplusplus
}
#endif

#endif /* DC_GUILD_H */
