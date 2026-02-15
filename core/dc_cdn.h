#ifndef DC_CDN_H
#define DC_CDN_H

/**
 * @file dc_cdn.h
 * @brief CDN URL builders and image helpers
 */

#include <stdint.h>
#include "dc_status.h"
#include "dc_string.h"
#include "dc_snowflake.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default Discord CDN base URL.
 */
#define DC_CDN_BASE_URL "https://cdn.discordapp.com"

/**
 * @brief Supported CDN image output formats.
 */
typedef enum {
    DC_CDN_IMAGE_PNG = 0, /**< PNG format */
    DC_CDN_IMAGE_JPG,     /**< JPEG/JPG format */
    DC_CDN_IMAGE_GIF,     /**< GIF format */
    DC_CDN_IMAGE_WEBP,    /**< WEBP format */
    DC_CDN_IMAGE_AVIF     /**< AVIF format */
} dc_cdn_image_format_t;

/** @brief Allowed format bit for PNG output. */
#define DC_CDN_FMT_PNG  (1u << 0)
/** @brief Allowed format bit for JPG output. */
#define DC_CDN_FMT_JPG  (1u << 1)
/** @brief Allowed format bit for GIF output. */
#define DC_CDN_FMT_GIF  (1u << 2)
/** @brief Allowed format bit for WEBP output. */
#define DC_CDN_FMT_WEBP (1u << 3)
/** @brief Allowed format bit for AVIF output. */
#define DC_CDN_FMT_AVIF (1u << 4)
/** @brief Bitmask containing all image formats. */
#define DC_CDN_FMT_ALL  (DC_CDN_FMT_PNG | DC_CDN_FMT_JPG | DC_CDN_FMT_GIF | DC_CDN_FMT_WEBP | DC_CDN_FMT_AVIF)

/**
 * @brief Check whether an image format enum value is supported.
 * @param format Image format to validate.
 * @return 1 if valid, 0 otherwise.
 */
int dc_cdn_image_format_is_valid(dc_cdn_image_format_t format);

/**
 * @brief Get lowercase file extension for an image format.
 * @param format Image format to convert.
 * @return Extension string without leading dot, or empty string if invalid.
 */
const char* dc_cdn_image_format_extension(dc_cdn_image_format_t format);

/**
 * @brief Validate image extension text.
 * @param ext Extension with or without leading dot.
 * @return 1 if supported, 0 otherwise.
 */
int dc_cdn_image_extension_is_valid(const char* ext);

/**
 * @brief Validate image size (power of two between 16 and 4096)
 * @param size Size to validate
 * @return 1 if valid, 0 otherwise
 */
int dc_cdn_image_size_is_valid(uint32_t size);

/**
 * @brief Check whether an asset hash marks an animated asset (`a_` prefix).
 * @param hash Asset hash text.
 * @return 1 if animated, 0 otherwise.
 */
int dc_cdn_hash_is_animated(const char* hash);

/**
 * @brief Build a CDN URL
 * @param base_url Base URL (NULL for default)
 * @param path_without_ext Path without extension (e.g., "/avatars/123/abc")
 * @param allowed_formats Bitmask of allowed formats (0 to skip check)
 * @param format Requested format
 * @param size Image size (0 to omit)
 * @param prefer_animated Prefer GIF if animated
 * @param is_animated Whether asset is animated
 * @param out Output URL
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_cdn_build_url(const char* base_url,
                             const char* path_without_ext,
                             uint32_t allowed_formats,
                             dc_cdn_image_format_t format,
                             uint32_t size,
                             int prefer_animated,
                             int is_animated,
                             dc_string_t* out);

/**
 * @brief Build a user avatar CDN URL.
 * @param user_id User snowflake.
 * @param hash Avatar hash.
 * @param format Requested image format.
 * @param size Image size (0 to omit query parameter).
 * @param prefer_animated Prefer GIF when hash indicates animation.
 * @param out Output URL.
 * @return DC_OK on success, error code on failure.
 */
dc_status_t dc_cdn_user_avatar(dc_snowflake_t user_id, const char* hash,
                               dc_cdn_image_format_t format, uint32_t size,
                               int prefer_animated, dc_string_t* out);

/**
 * @brief Build a guild icon CDN URL.
 * @param guild_id Guild snowflake.
 * @param hash Icon hash.
 * @param format Requested image format.
 * @param size Image size (0 to omit query parameter).
 * @param prefer_animated Prefer GIF when hash indicates animation.
 * @param out Output URL.
 * @return DC_OK on success, error code on failure.
 */
dc_status_t dc_cdn_guild_icon(dc_snowflake_t guild_id, const char* hash,
                              dc_cdn_image_format_t format, uint32_t size,
                              int prefer_animated, dc_string_t* out);

/**
 * @brief Build a group DM channel icon CDN URL.
 * @param channel_id Channel snowflake.
 * @param hash Icon hash.
 * @param format Requested image format.
 * @param size Image size (0 to omit query parameter).
 * @param prefer_animated Prefer GIF when hash indicates animation.
 * @param out Output URL.
 * @return DC_OK on success, error code on failure.
 */
dc_status_t dc_cdn_channel_icon(dc_snowflake_t channel_id, const char* hash,
                                dc_cdn_image_format_t format, uint32_t size,
                                int prefer_animated, dc_string_t* out);

/**
 * @brief Build a custom emoji CDN URL.
 * @param emoji_id Emoji snowflake.
 * @param animated Non-zero when emoji is animated.
 * @param format Requested image format.
 * @param size Image size (0 to omit query parameter).
 * @param out Output URL.
 * @return DC_OK on success, error code on failure.
 */
dc_status_t dc_cdn_emoji(dc_snowflake_t emoji_id, int animated,
                         dc_cdn_image_format_t format, uint32_t size,
                         dc_string_t* out);

/**
 * @brief Pass through signed attachment URLs without normalization
 * @param url Attachment URL.
 * @param out Output URL string.
 * @return DC_OK on success, error code on failure.
 */
dc_status_t dc_cdn_attachment_url_passthrough(const char* url, dc_string_t* out);

#ifdef __cplusplus
}
#endif

#endif /* DC_CDN_H */
