#ifndef DC_USER_H
#define DC_USER_H

/**
 * @file dc_user.h
 * @brief Discord User model
 */

#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_snowflake.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief User premium types
 */
typedef enum {
    DC_USER_PREMIUM_NONE = 0,
    DC_USER_PREMIUM_NITRO_CLASSIC = 1,
    DC_USER_PREMIUM_NITRO = 2,
    DC_USER_PREMIUM_NITRO_BASIC = 3
} dc_user_premium_type_t;

/**
 * @brief User flags
 */
typedef enum {
    DC_USER_FLAG_STAFF                    = 1 << 0,
    DC_USER_FLAG_PARTNER                  = 1 << 1,
    DC_USER_FLAG_HYPESQUAD                = 1 << 2,
    DC_USER_FLAG_BUG_HUNTER_LEVEL_1       = 1 << 3,
    DC_USER_FLAG_HYPESQUAD_ONLINE_HOUSE_1 = 1 << 6,
    DC_USER_FLAG_HYPESQUAD_ONLINE_HOUSE_2 = 1 << 7,
    DC_USER_FLAG_HYPESQUAD_ONLINE_HOUSE_3 = 1 << 8,
    DC_USER_FLAG_PREMIUM_EARLY_SUPPORTER  = 1 << 9,
    DC_USER_FLAG_TEAM_PSEUDO_USER         = 1 << 10,
    DC_USER_FLAG_BUG_HUNTER_LEVEL_2       = 1 << 14,
    DC_USER_FLAG_VERIFIED_BOT             = 1 << 16,
    DC_USER_FLAG_VERIFIED_DEVELOPER       = 1 << 17,
    DC_USER_FLAG_CERTIFIED_MODERATOR      = 1 << 18,
    DC_USER_FLAG_BOT_HTTP_INTERACTIONS    = 1 << 19,
    DC_USER_FLAG_ACTIVE_DEVELOPER         = 1 << 22
} dc_user_flag_t;

/**
 * @brief Discord User structure
 */
typedef struct {
    dc_snowflake_t id;                  /**< User ID */
    dc_string_t username;               /**< Username */
    dc_string_t discriminator;          /**< Discriminator (legacy) */
    dc_string_t global_name;            /**< Global display name */
    dc_string_t avatar;                 /**< Avatar hash */
    dc_string_t banner;                 /**< Banner hash */
    uint32_t accent_color;              /**< Accent color */
    dc_string_t locale;                 /**< User locale */
    dc_string_t email;                  /**< Email (if available) */
    uint32_t flags;                     /**< User flags */
    dc_user_premium_type_t premium_type; /**< Premium type */
    uint32_t public_flags;              /**< Public flags */
    dc_string_t avatar_decoration;      /**< Avatar decoration hash */
    int bot;                            /**< Is bot */
    int system;                         /**< Is system user */
    int mfa_enabled;                    /**< MFA enabled */
    int verified;                       /**< Email verified */
} dc_user_t;

/**
 * @brief Initialize user structure
 * @param user User to initialize
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_user_init(dc_user_t* user);

/**
 * @brief Free user structure
 * @param user User to free
 */
void dc_user_free(dc_user_t* user);

/**
 * @brief Parse user from JSON
 * @param json_data JSON data
 * @param user User to store result
 * @return DC_OK on success, error code on failure
 *
 * @note Caller must free any prior user data in @p user before reuse.
 */
dc_status_t dc_user_from_json(const char* json_data, dc_user_t* user);

/**
 * @brief Serialize user to JSON
 * @param user User to serialize
 * @param json_result String to store JSON result
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_user_to_json(const dc_user_t* user, dc_string_t* json_result);

/**
 * @brief Get user mention string
 * @param user User
 * @param mention String to store mention
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_user_get_mention(const dc_user_t* user, dc_string_t* mention);

/**
 * @brief Get user avatar URL
 * @param user User
 * @param size Image size (power of 2, 16-4096)
 * @param format Image format ("png", "jpg", "webp", "gif")
 * @param url String to store URL
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_user_get_avatar_url(const dc_user_t* user, uint16_t size, 
                                   const char* format, dc_string_t* url);

/**
 * @brief Get user default avatar URL
 * @param user User
 * @param size Image size (power of 2, 16-4096)
 * @param url String to store URL
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_user_get_default_avatar_url(const dc_user_t* user, uint16_t size, dc_string_t* url);

/**
 * @brief Get user banner URL
 * @param user User
 * @param size Image size (power of 2, 16-4096)
 * @param format Image format ("png", "jpg", "webp", "gif")
 * @param url String to store URL
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_user_get_banner_url(const dc_user_t* user, uint16_t size, 
                                   const char* format, dc_string_t* url);

/**
 * @brief Check if user has flag
 * @param user User
 * @param flag Flag to check
 * @return 1 if user has flag, 0 otherwise
 */
int dc_user_has_flag(const dc_user_t* user, dc_user_flag_t flag);

/**
 * @brief Get user display name (global_name or username)
 * @param user User
 * @return Display name string
 */
const char* dc_user_get_display_name(const dc_user_t* user);

#ifdef __cplusplus
}
#endif

#endif /* DC_USER_H */
