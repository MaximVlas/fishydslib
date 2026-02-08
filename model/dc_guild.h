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
#include "model/dc_model_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Discord Guild structure (v10 subset, safely extensible)
 */
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
