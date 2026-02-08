/**
 * @file dc_format.c
 * @brief Message formatting helpers
 */

#include "dc_format.h"
#include <inttypes.h>
#include <string.h>

static dc_status_t dc_format_build_with_id(const char* prefix, const char* suffix,
                                           dc_snowflake_t id, dc_string_t* out) {
    if (!prefix || !suffix || !out) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(id)) return DC_ERROR_INVALID_PARAM;

    char buf[32];
    dc_status_t st = dc_snowflake_to_cstr(id, buf, sizeof(buf));
    if (st != DC_OK) return st;

    dc_string_t tmp;
    st = dc_string_init(&tmp);
    if (st != DC_OK) return st;
    st = dc_string_append_cstr(&tmp, prefix);
    if (st != DC_OK) goto fail;
    st = dc_string_append_cstr(&tmp, buf);
    if (st != DC_OK) goto fail;
    st = dc_string_append_cstr(&tmp, suffix);
    if (st != DC_OK) goto fail;

    dc_string_free(out);
    *out = tmp;
    return DC_OK;

fail:
    dc_string_free(&tmp);
    return st;
}

int dc_format_timestamp_style_is_valid(char style) {
    if (style == '\0') return 1;
    switch (style) {
        case 't':
        case 'T':
        case 'd':
        case 'D':
        case 'f':
        case 'F':
        case 'R':
            return 1;
        default:
            return 0;
    }
}

dc_status_t dc_format_mention_user(dc_snowflake_t user_id, dc_string_t* out) {
    return dc_format_build_with_id("<@", ">", user_id, out);
}

dc_status_t dc_format_mention_user_nick(dc_snowflake_t user_id, dc_string_t* out) {
    return dc_format_build_with_id("<@!", ">", user_id, out);
}

dc_status_t dc_format_mention_channel(dc_snowflake_t channel_id, dc_string_t* out) {
    return dc_format_build_with_id("<#", ">", channel_id, out);
}

dc_status_t dc_format_mention_role(dc_snowflake_t role_id, dc_string_t* out) {
    return dc_format_build_with_id("<@&", ">", role_id, out);
}

dc_status_t dc_format_slash_command_mention(const char* name, dc_snowflake_t command_id,
                                            dc_string_t* out) {
    if (!name || !out) return DC_ERROR_NULL_POINTER;
    if (name[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (!dc_snowflake_is_valid(command_id)) return DC_ERROR_INVALID_PARAM;
    if (strchr(name, '<') || strchr(name, '>') || strchr(name, ':')) {
        return DC_ERROR_INVALID_PARAM;
    }

    char id_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(command_id, id_buf, sizeof(id_buf));
    if (st != DC_OK) return st;

    dc_string_t tmp;
    st = dc_string_init(&tmp);
    if (st != DC_OK) return st;
    st = dc_string_append_cstr(&tmp, "</");
    if (st != DC_OK) goto fail;
    st = dc_string_append_cstr(&tmp, name);
    if (st != DC_OK) goto fail;
    st = dc_string_append_cstr(&tmp, ":");
    if (st != DC_OK) goto fail;
    st = dc_string_append_cstr(&tmp, id_buf);
    if (st != DC_OK) goto fail;
    st = dc_string_append_cstr(&tmp, ">");
    if (st != DC_OK) goto fail;

    dc_string_free(out);
    *out = tmp;
    return DC_OK;

fail:
    dc_string_free(&tmp);
    return st;
}

static int dc_format_name_is_valid(const char* name) {
    if (!name || name[0] == '\0') return 0;
    const unsigned char* p = (const unsigned char*)name;
    while (*p) {
        unsigned char c = *p++;
        if (c <= 0x20) return 0;
        if (c == '<' || c == '>' || c == ':') return 0;
    }
    return 1;
}

dc_status_t dc_format_mention_emoji(const char* name, dc_snowflake_t emoji_id, int animated,
                                    dc_string_t* out) {
    if (!name || !out) return DC_ERROR_NULL_POINTER;
    if (!dc_format_name_is_valid(name)) return DC_ERROR_INVALID_PARAM;
    if (!dc_snowflake_is_valid(emoji_id)) return DC_ERROR_INVALID_PARAM;

    char id_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(emoji_id, id_buf, sizeof(id_buf));
    if (st != DC_OK) return st;

    dc_string_t tmp;
    st = dc_string_init(&tmp);
    if (st != DC_OK) return st;

    st = dc_string_append_cstr(&tmp, animated ? "<a:" : "<:");
    if (st != DC_OK) goto fail;
    st = dc_string_append_cstr(&tmp, name);
    if (st != DC_OK) goto fail;
    st = dc_string_append_cstr(&tmp, ":");
    if (st != DC_OK) goto fail;
    st = dc_string_append_cstr(&tmp, id_buf);
    if (st != DC_OK) goto fail;
    st = dc_string_append_cstr(&tmp, ">");
    if (st != DC_OK) goto fail;

    dc_string_free(out);
    *out = tmp;
    return DC_OK;

fail:
    dc_string_free(&tmp);
    return st;
}

dc_status_t dc_format_timestamp(int64_t unix_seconds, char style, dc_string_t* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (unix_seconds < 0) return DC_ERROR_INVALID_PARAM;
    if (!dc_format_timestamp_style_is_valid(style)) return DC_ERROR_INVALID_PARAM;

    dc_string_t tmp;
    dc_status_t st = dc_string_init(&tmp);
    if (st != DC_OK) return st;

    if (style == '\0') {
        st = dc_string_printf(&tmp, "<t:%" PRId64 ">", unix_seconds);
    } else {
        st = dc_string_printf(&tmp, "<t:%" PRId64 ":%c>", unix_seconds, style);
    }
    if (st != DC_OK) {
        dc_string_free(&tmp);
        return st;
    }
    dc_string_free(out);
    *out = tmp;
    return DC_OK;
}

dc_status_t dc_format_timestamp_ms(int64_t unix_ms, char style, dc_string_t* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (unix_ms < 0) return DC_ERROR_INVALID_PARAM;
    int64_t seconds = unix_ms / 1000;
    return dc_format_timestamp(seconds, style, out);
}

static int dc_format_should_escape(char c) {
    switch (c) {
        case '\\':
        case '*':
        case '_':
        case '~':
        case '`':
        case '|':
        case '<':
        case '>':
        case '@':
        case '#':
            return 1;
        default:
            return 0;
    }
}

dc_status_t dc_format_escape_content(const char* input, dc_string_t* out) {
    if (!input || !out) return DC_ERROR_NULL_POINTER;

    dc_string_t tmp;
    dc_status_t st = dc_string_init(&tmp);
    if (st != DC_OK) return st;

    const unsigned char* p = (const unsigned char*)input;
    while (*p) {
        char c = (char)*p++;
        if (dc_format_should_escape(c)) {
            st = dc_string_append_char(&tmp, '\\');
            if (st != DC_OK) goto fail;
        }
        st = dc_string_append_char(&tmp, c);
        if (st != DC_OK) goto fail;
    }

    dc_string_free(out);
    *out = tmp;
    return DC_OK;

fail:
    dc_string_free(&tmp);
    return st;
}
