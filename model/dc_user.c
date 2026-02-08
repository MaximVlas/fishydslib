/**
 * @file dc_user.c
 * @brief User model implementation
 */

#include "dc_user.h"
#include "json/dc_json.h"
#include "json/dc_json_model.h"
#include <string.h>

static int dc_user_avatar_size_valid(uint16_t size) {
    if (size < 16 || size > 4096) return 0;
    return (size & (size - 1)) == 0;
}

static int dc_user_avatar_format_valid(const char* format) {
    if (!format) return 0;
    return strcmp(format, "png") == 0 ||
           strcmp(format, "jpg") == 0 ||
           strcmp(format, "jpeg") == 0 ||
           strcmp(format, "webp") == 0 ||
           strcmp(format, "gif") == 0;
}

dc_status_t dc_user_init(dc_user_t* user) {
    if (!user) return DC_ERROR_NULL_POINTER;
    memset(user, 0, sizeof(*user));
    dc_status_t st = dc_string_init(&user->username);
    if (st != DC_OK) return st;
    st = dc_string_init(&user->discriminator);
    if (st != DC_OK) return st;
    st = dc_string_init(&user->global_name);
    if (st != DC_OK) return st;
    st = dc_string_init(&user->avatar);
    if (st != DC_OK) return st;
    st = dc_string_init(&user->banner);
    if (st != DC_OK) return st;
    st = dc_string_init(&user->locale);
    if (st != DC_OK) return st;
    st = dc_string_init(&user->email);
    if (st != DC_OK) return st;
    st = dc_string_init(&user->avatar_decoration);
    if (st != DC_OK) return st;
    return DC_OK;
}

void dc_user_free(dc_user_t* user) {
    if (!user) return;
    dc_string_free(&user->username);
    dc_string_free(&user->discriminator);
    dc_string_free(&user->global_name);
    dc_string_free(&user->avatar);
    dc_string_free(&user->banner);
    dc_string_free(&user->locale);
    dc_string_free(&user->email);
    dc_string_free(&user->avatar_decoration);
    memset(user, 0, sizeof(*user));
}

dc_status_t dc_user_from_json(const char* json_data, dc_user_t* user) {
    if (!json_data || !user) return DC_ERROR_NULL_POINTER;
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(json_data, &doc);
    if (st != DC_OK) return st;

    dc_user_t tmp;
    st = dc_user_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    st = dc_json_model_user_from_val(doc.root, &tmp);
    dc_json_doc_free(&doc);
    if (st != DC_OK) {
        dc_user_free(&tmp);
        return st;
    }
    *user = tmp;
    return DC_OK;
}

dc_status_t dc_user_to_json(const dc_user_t* user, dc_string_t* json_result) {
    if (!user || !json_result) return DC_ERROR_NULL_POINTER;
    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;

    st = dc_json_model_user_to_mut(&doc, doc.root, user);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }

    st = dc_json_mut_doc_serialize(&doc, json_result);
    dc_json_mut_doc_free(&doc);
    return st;
}

dc_status_t dc_user_get_mention(const dc_user_t* user, dc_string_t* mention) {
    if (!user || !mention) return DC_ERROR_NULL_POINTER;
    char buf[32];
    dc_status_t st = dc_snowflake_to_cstr(user->id, buf, sizeof(buf));
    if (st != DC_OK) return st;
    return dc_string_printf(mention, "<@%s>", buf);
}

dc_status_t dc_user_get_avatar_url(const dc_user_t* user, uint16_t size, const char* format, dc_string_t* url) {
    if (!user || !url || !format) return DC_ERROR_NULL_POINTER;
    if (!dc_user_avatar_size_valid(size)) return DC_ERROR_INVALID_PARAM;
    if (!dc_user_avatar_format_valid(format)) return DC_ERROR_INVALID_PARAM;
    if (dc_string_is_empty(&user->avatar)) return DC_ERROR_NOT_FOUND;

    char id_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(user->id, id_buf, sizeof(id_buf));
    if (st != DC_OK) return st;
    return dc_string_printf(url, "https://cdn.discordapp.com/avatars/%s/%s.%s?size=%u",
                            id_buf, dc_string_cstr(&user->avatar), format, (unsigned)size);
}

dc_status_t dc_user_get_default_avatar_url(const dc_user_t* user, uint16_t size, dc_string_t* url) {
    if (!user || !url) return DC_ERROR_NULL_POINTER;
    if (!dc_user_avatar_size_valid(size)) return DC_ERROR_INVALID_PARAM;

    unsigned index = 0;
    const char* disc = dc_string_cstr(&user->discriminator);
    if (disc && disc[0] != '\0' && strcmp(disc, "0") != 0) {
        unsigned disc_val = 0;
        for (const char* p = disc; *p; ++p) {
            if (*p < '0' || *p > '9') { disc_val = 0; break; }
            disc_val = disc_val * 10u + (unsigned)(*p - '0');
        }
        index = disc_val % 5u;
    } else {
        index = (unsigned)((user->id >> 22) % 6u);
    }

    return dc_string_printf(url, "https://cdn.discordapp.com/embed/avatars/%u.png?size=%u",
                            index, (unsigned)size);
}

dc_status_t dc_user_get_banner_url(const dc_user_t* user, uint16_t size, const char* format, dc_string_t* url) {
    if (!user || !url || !format) return DC_ERROR_NULL_POINTER;
    if (!dc_user_avatar_size_valid(size)) return DC_ERROR_INVALID_PARAM;
    if (!dc_user_avatar_format_valid(format)) return DC_ERROR_INVALID_PARAM;
    if (dc_string_is_empty(&user->banner)) return DC_ERROR_NOT_FOUND;

    char id_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(user->id, id_buf, sizeof(id_buf));
    if (st != DC_OK) return st;
    return dc_string_printf(url, "https://cdn.discordapp.com/banners/%s/%s.%s?size=%u",
                            id_buf, dc_string_cstr(&user->banner), format, (unsigned)size);
}

int dc_user_has_flag(const dc_user_t* user, dc_user_flag_t flag) {
    if (!user) return 0;
    return (user->flags & (uint32_t)flag) != 0;
}

const char* dc_user_get_display_name(const dc_user_t* user) {
    if (!user) return "";
    if (!dc_string_is_empty(&user->global_name)) return dc_string_cstr(&user->global_name);
    return dc_string_cstr(&user->username);
}
