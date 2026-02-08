/**
 * @file dc_cdn.c
 * @brief CDN URL builders and image helpers
 */

#include "dc_cdn.h"
#include "dc_string.h"
#include <string.h>

static int dc_cdn_ascii_tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int dc_cdn_ascii_strcaseeq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        int ca = dc_cdn_ascii_tolower((unsigned char)*a);
        int cb = dc_cdn_ascii_tolower((unsigned char)*b);
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

int dc_cdn_image_format_is_valid(dc_cdn_image_format_t format) {
    switch (format) {
        case DC_CDN_IMAGE_PNG:
        case DC_CDN_IMAGE_JPG:
        case DC_CDN_IMAGE_GIF:
        case DC_CDN_IMAGE_WEBP:
        case DC_CDN_IMAGE_AVIF:
            return 1;
        default:
            return 0;
    }
}

const char* dc_cdn_image_format_extension(dc_cdn_image_format_t format) {
    switch (format) {
        case DC_CDN_IMAGE_PNG:
            return "png";
        case DC_CDN_IMAGE_JPG:
            return "jpg";
        case DC_CDN_IMAGE_GIF:
            return "gif";
        case DC_CDN_IMAGE_WEBP:
            return "webp";
        case DC_CDN_IMAGE_AVIF:
            return "avif";
        default:
            return "";
    }
}

int dc_cdn_image_extension_is_valid(const char* ext) {
    if (!ext || ext[0] == '\0') return 0;
    if (ext[0] == '.') ext++;
    return dc_cdn_ascii_strcaseeq(ext, "png") ||
           dc_cdn_ascii_strcaseeq(ext, "jpg") ||
           dc_cdn_ascii_strcaseeq(ext, "jpeg") ||
           dc_cdn_ascii_strcaseeq(ext, "gif") ||
           dc_cdn_ascii_strcaseeq(ext, "webp") ||
           dc_cdn_ascii_strcaseeq(ext, "avif");
}

int dc_cdn_image_size_is_valid(uint32_t size) {
    if (size < 16 || size > 4096) return 0;
    return (size & (size - 1)) == 0;
}

int dc_cdn_hash_is_animated(const char* hash) {
    if (!hash) return 0;
    return (hash[0] == 'a' && hash[1] == '_' && hash[2] != '\0');
}

static uint32_t dc_cdn_format_mask(dc_cdn_image_format_t format) {
    switch (format) {
        case DC_CDN_IMAGE_PNG:
            return DC_CDN_FMT_PNG;
        case DC_CDN_IMAGE_JPG:
            return DC_CDN_FMT_JPG;
        case DC_CDN_IMAGE_GIF:
            return DC_CDN_FMT_GIF;
        case DC_CDN_IMAGE_WEBP:
            return DC_CDN_FMT_WEBP;
        case DC_CDN_IMAGE_AVIF:
            return DC_CDN_FMT_AVIF;
        default:
            return 0;
    }
}

static dc_status_t dc_cdn_append_path(dc_string_t* out, const char* base, const char* path) {
    if (!out || !base || !path) return DC_ERROR_NULL_POINTER;
    size_t base_len = strlen(base);
    int base_has_slash = (base_len > 0 && base[base_len - 1] == '/');
    int path_has_slash = (path[0] == '/');

    dc_status_t st = dc_string_append_cstr(out, base);
    if (st != DC_OK) return st;

    if (base_has_slash && path_has_slash) {
        return dc_string_append_cstr(out, path + 1);
    }
    if (!base_has_slash && !path_has_slash) {
        st = dc_string_append_char(out, '/');
        if (st != DC_OK) return st;
    }
    return dc_string_append_cstr(out, path);
}

dc_status_t dc_cdn_build_url(const char* base_url,
                             const char* path_without_ext,
                             uint32_t allowed_formats,
                             dc_cdn_image_format_t format,
                             uint32_t size,
                             int prefer_animated,
                             int is_animated,
                             dc_string_t* out) {
    if (!path_without_ext || !out) return DC_ERROR_NULL_POINTER;
    if (path_without_ext[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (!dc_cdn_image_format_is_valid(format)) return DC_ERROR_INVALID_PARAM;
    if (size > 0 && !dc_cdn_image_size_is_valid(size)) return DC_ERROR_INVALID_PARAM;

    uint32_t fmt_mask = dc_cdn_format_mask(format);
    if (allowed_formats && (fmt_mask == 0 || (allowed_formats & fmt_mask) == 0)) {
        return DC_ERROR_INVALID_PARAM;
    }

    const char* base = (base_url && base_url[0] != '\0') ? base_url : DC_CDN_BASE_URL;
    const char* ext = dc_cdn_image_format_extension(format);

    if (prefer_animated && is_animated) {
        if (allowed_formats == 0 || (allowed_formats & DC_CDN_FMT_GIF)) {
            ext = "gif";
        }
    }

    dc_string_t tmp;
    dc_status_t st = dc_string_init(&tmp);
    if (st != DC_OK) return st;
    st = dc_cdn_append_path(&tmp, base, path_without_ext);
    if (st != DC_OK) goto fail;
    st = dc_string_append_char(&tmp, '.');
    if (st != DC_OK) goto fail;
    st = dc_string_append_cstr(&tmp, ext);
    if (st != DC_OK) goto fail;

    if (size > 0) {
        st = dc_string_append_printf(&tmp, "?size=%u", (unsigned)size);
        if (st != DC_OK) goto fail;
    }

    dc_string_free(out);
    *out = tmp;
    return DC_OK;

fail:
    dc_string_free(&tmp);
    return st;
}

static dc_status_t dc_cdn_build_asset_url(const char* path_prefix,
                                          dc_snowflake_t id,
                                          const char* hash,
                                          dc_cdn_image_format_t format,
                                          uint32_t size,
                                          int prefer_animated,
                                          dc_string_t* out) {
    if (!path_prefix || !hash || !out) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(id)) return DC_ERROR_INVALID_PARAM;
    if (hash[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char id_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(id, id_buf, sizeof(id_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "%s/%s/%s", path_prefix, id_buf, hash);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    int animated = dc_cdn_hash_is_animated(hash);
    st = dc_cdn_build_url(NULL, dc_string_cstr(&path), DC_CDN_FMT_ALL, format, size,
                          prefer_animated, animated, out);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_cdn_user_avatar(dc_snowflake_t user_id, const char* hash,
                               dc_cdn_image_format_t format, uint32_t size,
                               int prefer_animated, dc_string_t* out) {
    return dc_cdn_build_asset_url("/avatars", user_id, hash, format, size, prefer_animated, out);
}

dc_status_t dc_cdn_guild_icon(dc_snowflake_t guild_id, const char* hash,
                              dc_cdn_image_format_t format, uint32_t size,
                              int prefer_animated, dc_string_t* out) {
    return dc_cdn_build_asset_url("/icons", guild_id, hash, format, size, prefer_animated, out);
}

dc_status_t dc_cdn_channel_icon(dc_snowflake_t channel_id, const char* hash,
                                dc_cdn_image_format_t format, uint32_t size,
                                int prefer_animated, dc_string_t* out) {
    return dc_cdn_build_asset_url("/channel-icons", channel_id, hash, format, size, prefer_animated, out);
}

dc_status_t dc_cdn_emoji(dc_snowflake_t emoji_id, int animated,
                         dc_cdn_image_format_t format, uint32_t size,
                         dc_string_t* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(emoji_id)) return DC_ERROR_INVALID_PARAM;

    char id_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(emoji_id, id_buf, sizeof(id_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/emojis/%s", id_buf);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_cdn_build_url(NULL, dc_string_cstr(&path), DC_CDN_FMT_ALL, format, size,
                          animated ? 1 : 0, animated ? 1 : 0, out);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_cdn_attachment_url_passthrough(const char* url, dc_string_t* out) {
    if (!url || !out) return DC_ERROR_NULL_POINTER;
    if (url[0] == '\0') return DC_ERROR_INVALID_PARAM;
    return dc_string_set_cstr(out, url);
}
