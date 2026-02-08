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

#define DC_CDN_BASE_URL "https://cdn.discordapp.com"

typedef enum {
    DC_CDN_IMAGE_PNG = 0,
    DC_CDN_IMAGE_JPG,
    DC_CDN_IMAGE_GIF,
    DC_CDN_IMAGE_WEBP,
    DC_CDN_IMAGE_AVIF
} dc_cdn_image_format_t;

#define DC_CDN_FMT_PNG  (1u << 0)
#define DC_CDN_FMT_JPG  (1u << 1)
#define DC_CDN_FMT_GIF  (1u << 2)
#define DC_CDN_FMT_WEBP (1u << 3)
#define DC_CDN_FMT_AVIF (1u << 4)
#define DC_CDN_FMT_ALL  (DC_CDN_FMT_PNG | DC_CDN_FMT_JPG | DC_CDN_FMT_GIF | DC_CDN_FMT_WEBP | DC_CDN_FMT_AVIF)

int dc_cdn_image_format_is_valid(dc_cdn_image_format_t format);
const char* dc_cdn_image_format_extension(dc_cdn_image_format_t format);
int dc_cdn_image_extension_is_valid(const char* ext);

/**
 * @brief Validate image size (power of two between 16 and 4096)
 * @param size Size to validate
 * @return 1 if valid, 0 otherwise
 */
int dc_cdn_image_size_is_valid(uint32_t size);

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
 */
dc_status_t dc_cdn_build_url(const char* base_url,
                             const char* path_without_ext,
                             uint32_t allowed_formats,
                             dc_cdn_image_format_t format,
                             uint32_t size,
                             int prefer_animated,
                             int is_animated,
                             dc_string_t* out);

dc_status_t dc_cdn_user_avatar(dc_snowflake_t user_id, const char* hash,
                               dc_cdn_image_format_t format, uint32_t size,
                               int prefer_animated, dc_string_t* out);

dc_status_t dc_cdn_guild_icon(dc_snowflake_t guild_id, const char* hash,
                              dc_cdn_image_format_t format, uint32_t size,
                              int prefer_animated, dc_string_t* out);

dc_status_t dc_cdn_channel_icon(dc_snowflake_t channel_id, const char* hash,
                                dc_cdn_image_format_t format, uint32_t size,
                                int prefer_animated, dc_string_t* out);

dc_status_t dc_cdn_emoji(dc_snowflake_t emoji_id, int animated,
                         dc_cdn_image_format_t format, uint32_t size,
                         dc_string_t* out);

/**
 * @brief Pass through signed attachment URLs without normalization
 */
dc_status_t dc_cdn_attachment_url_passthrough(const char* url, dc_string_t* out);

#ifdef __cplusplus
}
#endif

#endif /* DC_CDN_H */
