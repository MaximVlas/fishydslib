/**
 * @file dc_data_uri.c
 * @brief Data URI helpers for image payloads
 */

#include "dc_data_uri.h"
#include <string.h>

static int dc_data_uri_is_base64_char(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= '0' && c <= '9') return 1;
    if (c == '+' || c == '/') return 1;
    return 0;
}

static int dc_data_uri_base64_is_valid(const char* s) {
    if (!s || *s == '\0') return 0;
    size_t len = 0;
    size_t padding = 0;
    int padding_started = 0;

    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        unsigned char c = *p;
        len++;
        if (c == '=') {
            padding++;
            padding_started = 1;
            if (padding > 2) return 0;
            continue;
        }
        if (!dc_data_uri_is_base64_char(c)) return 0;
        if (padding_started) return 0;
    }
    if (len % 4 != 0) return 0;
    return 1;
}

static int dc_data_uri_format_is_valid(const char* fmt) {
    if (!fmt) return 0;
    if (strcmp(fmt, "png") == 0) return 1;
    if (strcmp(fmt, "jpg") == 0) return 1;
    if (strcmp(fmt, "jpeg") == 0) return 1;
    if (strcmp(fmt, "gif") == 0) return 1;
    if (strcmp(fmt, "webp") == 0) return 1;
    if (strcmp(fmt, "avif") == 0) return 1;
    return 0;
}

int dc_data_uri_is_valid_image_base64(const char* data_uri) {
    if (!data_uri) return 0;
    const char* prefix = "data:image/";
    size_t prefix_len = strlen(prefix);
    if (strncmp(data_uri, prefix, prefix_len) != 0) return 0;

    const char* fmt_start = data_uri + prefix_len;
    const char* semi = strchr(fmt_start, ';');
    if (!semi || semi == fmt_start) return 0;

    size_t fmt_len = (size_t)(semi - fmt_start);
    char fmt_buf[16];
    if (fmt_len >= sizeof(fmt_buf)) return 0;
    memcpy(fmt_buf, fmt_start, fmt_len);
    fmt_buf[fmt_len] = '\0';
    if (!dc_data_uri_format_is_valid(fmt_buf)) return 0;

    const char* base = ";base64,";
    size_t base_len = strlen(base);
    if (strncmp(semi, base, base_len) != 0) return 0;

    const char* payload = semi + base_len;
    return dc_data_uri_base64_is_valid(payload);
}

static const char* dc_data_uri_mime_for_format(dc_cdn_image_format_t format) {
    switch (format) {
        case DC_CDN_IMAGE_PNG:
            return "png";
        case DC_CDN_IMAGE_JPG:
            return "jpeg";
        case DC_CDN_IMAGE_GIF:
            return "gif";
        case DC_CDN_IMAGE_WEBP:
            return "webp";
        case DC_CDN_IMAGE_AVIF:
            return "avif";
        default:
            return NULL;
    }
}

dc_status_t dc_data_uri_build_image_base64(dc_cdn_image_format_t format,
                                           const char* base64,
                                           dc_string_t* out) {
    if (!base64 || !out) return DC_ERROR_NULL_POINTER;
    if (!dc_data_uri_base64_is_valid(base64)) return DC_ERROR_INVALID_PARAM;

    const char* mime = dc_data_uri_mime_for_format(format);
    if (!mime) return DC_ERROR_INVALID_PARAM;

    return dc_string_printf(out, "data:image/%s;base64,%s", mime, base64);
}
