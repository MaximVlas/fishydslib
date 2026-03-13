/**
 * @file dc_guild.c
 * @brief Guild model implementation
 */

#include "dc_guild.h"
#include "json/dc_json.h"
#include "json/dc_json_model.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

static dc_status_t dc_guild_get_nullable_string(yyjson_val* obj,
                                                const char* key,
                                                dc_nullable_string_t* out);
static dc_status_t dc_guild_get_optional_bool(yyjson_val* obj,
                                              const char* key,
                                              dc_optional_bool_t* out);
static dc_status_t dc_guild_get_optional_i32(yyjson_val* obj,
                                             const char* key,
                                             dc_optional_i32_t* out);
static dc_status_t dc_guild_get_optional_snowflake(yyjson_val* obj,
                                                   const char* key,
                                                   dc_optional_snowflake_t* out);
static dc_status_t dc_guild_add_nullable_string(dc_json_mut_doc_t* doc,
                                                yyjson_mut_val* obj,
                                                const char* key,
                                                const dc_nullable_string_t* val);
static dc_status_t dc_guild_add_optional_bool(dc_json_mut_doc_t* doc,
                                              yyjson_mut_val* obj,
                                              const char* key,
                                              const dc_optional_bool_t* val);
static dc_status_t dc_guild_add_optional_i32(dc_json_mut_doc_t* doc,
                                             yyjson_mut_val* obj,
                                             const char* key,
                                             const dc_optional_i32_t* val);
static dc_status_t dc_guild_add_optional_snowflake(dc_json_mut_doc_t* doc,
                                                   yyjson_mut_val* obj,
                                                   const char* key,
                                                   const dc_optional_snowflake_t* val);

static dc_status_t dc_guild_i64_to_int_checked(int64_t val, int* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (val < (int64_t)INT_MIN || val > (int64_t)INT_MAX) return DC_ERROR_INVALID_FORMAT;
    *out = (int)val;
    return DC_OK;
}

static dc_status_t dc_guild_i64_to_i32_checked(int64_t val, int32_t* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (val < (int64_t)INT32_MIN || val > (int64_t)INT32_MAX) return DC_ERROR_INVALID_FORMAT;
    *out = (int32_t)val;
    return DC_OK;
}

static dc_status_t dc_guild_u64_to_i64_checked(uint64_t val, int64_t* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (val > (uint64_t)INT64_MAX) return DC_ERROR_INVALID_PARAM;
    *out = (int64_t)val;
    return DC_OK;
}

static dc_status_t dc_guild_copy_cstr(dc_string_t* dst, const char* src) {
    if (!dst) return DC_ERROR_NULL_POINTER;
    return dc_string_set_cstr(dst, src ? src : "");
}

static dc_status_t dc_guild_copy_raw_json(yyjson_val* val, dc_string_t* out) {
    return dc_json_write_value_to_string(val, YYJSON_WRITE_NOFLAG, out);
}

static dc_status_t dc_guild_parse_features(yyjson_val* obj,
                                           int* has_features,
                                           dc_vec_t* features) {
    if (!obj || !has_features || !features) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, "features");
    if (!field || yyjson_is_null(field)) {
        *has_features = 0;
        return DC_OK;
    }
    if (!yyjson_is_arr(field)) return DC_ERROR_INVALID_FORMAT;

    *has_features = 1;
    size_t idx = 0, max = 0;
    yyjson_val* item = NULL;
    yyjson_arr_foreach(field, idx, max, item) {
        if (!yyjson_is_str(item)) return DC_ERROR_INVALID_FORMAT;

        dc_string_t feature;
        dc_status_t st = dc_string_init(&feature);
        if (st != DC_OK) return st;
        st = dc_string_set_cstr(&feature, yyjson_get_str(item));
        if (st != DC_OK) {
            dc_string_free(&feature);
            return st;
        }
        st = dc_vec_push(features, &feature);
        if (st != DC_OK) {
            dc_string_free(&feature);
            return st;
        }
    }

    return DC_OK;
}

static dc_status_t dc_guild_parse_roles(yyjson_val* obj, dc_role_list_t* roles) {
    if (!obj || !roles) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, "roles");
    if (!field || yyjson_is_null(field)) return DC_OK;
    if (!yyjson_is_arr(field)) return DC_ERROR_INVALID_FORMAT;

    size_t idx = 0, max = 0;
    yyjson_val* item = NULL;
    yyjson_arr_foreach(field, idx, max, item) {
        if (!yyjson_is_obj(item)) return DC_ERROR_INVALID_FORMAT;

        dc_role_t role;
        dc_status_t st = dc_role_init(&role);
        if (st != DC_OK) return st;

        st = dc_json_model_role_from_val(item, &role);
        if (st != DC_OK) {
            dc_role_free(&role);
            return st;
        }

        st = dc_vec_push(&roles->items, &role);
        if (st != DC_OK) {
            dc_role_free(&role);
            return st;
        }
    }

    return DC_OK;
}

static dc_status_t dc_guild_emoji_init(dc_guild_emoji_t* emoji) {
    if (!emoji) return DC_ERROR_NULL_POINTER;
    memset(emoji, 0, sizeof(*emoji));
    dc_optional_snowflake_clear(&emoji->id);
    dc_optional_bool_clear(&emoji->require_colons);
    dc_optional_bool_clear(&emoji->managed);
    dc_optional_bool_clear(&emoji->animated);
    dc_optional_bool_clear(&emoji->available);

    dc_status_t st = dc_nullable_string_init(&emoji->name);
    if (st != DC_OK) return st;
    st = dc_vec_init(&emoji->roles, sizeof(dc_snowflake_t));
    if (st != DC_OK) return st;
    st = dc_user_init(&emoji->user);
    if (st != DC_OK) return st;
    return DC_OK;
}

static void dc_guild_emoji_free(dc_guild_emoji_t* emoji) {
    if (!emoji) return;
    dc_nullable_string_free(&emoji->name);
    dc_vec_free(&emoji->roles);
    dc_user_free(&emoji->user);
    memset(emoji, 0, sizeof(*emoji));
}

static dc_status_t dc_guild_emoji_list_init(dc_guild_emoji_list_t* list) {
    if (!list) return DC_ERROR_NULL_POINTER;
    return dc_vec_init(&list->items, sizeof(dc_guild_emoji_t));
}

static void dc_guild_emoji_list_free(dc_guild_emoji_list_t* list) {
    if (!list) return;
    for (size_t i = 0; i < dc_vec_length(&list->items); i++) {
        dc_guild_emoji_t* emoji = (dc_guild_emoji_t*)dc_vec_at(&list->items, i);
        if (emoji) dc_guild_emoji_free(emoji);
    }
    dc_vec_free(&list->items);
}

static dc_status_t dc_guild_sticker_init(dc_guild_sticker_t* sticker) {
    if (!sticker) return DC_ERROR_NULL_POINTER;
    memset(sticker, 0, sizeof(*sticker));
    dc_optional_snowflake_clear(&sticker->pack_id);
    dc_optional_snowflake_clear(&sticker->guild_id);
    dc_optional_bool_clear(&sticker->available);
    dc_optional_i32_clear(&sticker->sort_value);

    dc_status_t st = dc_string_init(&sticker->name);
    if (st != DC_OK) return st;
    st = dc_nullable_string_init(&sticker->description);
    if (st != DC_OK) return st;
    st = dc_string_init(&sticker->tags);
    if (st != DC_OK) return st;
    st = dc_user_init(&sticker->user);
    if (st != DC_OK) return st;
    return DC_OK;
}

static void dc_guild_sticker_free(dc_guild_sticker_t* sticker) {
    if (!sticker) return;
    dc_string_free(&sticker->name);
    dc_nullable_string_free(&sticker->description);
    dc_string_free(&sticker->tags);
    dc_user_free(&sticker->user);
    memset(sticker, 0, sizeof(*sticker));
}

static dc_status_t dc_guild_sticker_list_init(dc_guild_sticker_list_t* list) {
    if (!list) return DC_ERROR_NULL_POINTER;
    return dc_vec_init(&list->items, sizeof(dc_guild_sticker_t));
}

static void dc_guild_sticker_list_free(dc_guild_sticker_list_t* list) {
    if (!list) return;
    for (size_t i = 0; i < dc_vec_length(&list->items); i++) {
        dc_guild_sticker_t* sticker = (dc_guild_sticker_t*)dc_vec_at(&list->items, i);
        if (sticker) dc_guild_sticker_free(sticker);
    }
    dc_vec_free(&list->items);
}

static dc_status_t dc_guild_welcome_channel_init(dc_guild_welcome_channel_t* channel) {
    if (!channel) return DC_ERROR_NULL_POINTER;
    memset(channel, 0, sizeof(*channel));
    dc_nullable_snowflake_set_null(&channel->emoji_id);
    dc_status_t st = dc_string_init(&channel->description);
    if (st != DC_OK) return st;
    st = dc_nullable_string_init(&channel->emoji_name);
    if (st != DC_OK) return st;
    return DC_OK;
}

static void dc_guild_welcome_channel_free(dc_guild_welcome_channel_t* channel) {
    if (!channel) return;
    dc_string_free(&channel->description);
    dc_nullable_string_free(&channel->emoji_name);
    dc_nullable_snowflake_set_null(&channel->emoji_id);
    memset(channel, 0, sizeof(*channel));
}

static dc_status_t dc_guild_welcome_screen_init(dc_guild_welcome_screen_t* ws) {
    if (!ws) return DC_ERROR_NULL_POINTER;
    memset(ws, 0, sizeof(*ws));
    dc_status_t st = dc_nullable_string_init(&ws->description);
    if (st != DC_OK) return st;
    st = dc_vec_init(&ws->welcome_channels, sizeof(dc_guild_welcome_channel_t));
    if (st != DC_OK) return st;
    return DC_OK;
}

static void dc_guild_welcome_screen_free(dc_guild_welcome_screen_t* ws) {
    if (!ws) return;
    dc_nullable_string_free(&ws->description);
    for (size_t i = 0; i < dc_vec_length(&ws->welcome_channels); i++) {
        dc_guild_welcome_channel_t* channel =
            (dc_guild_welcome_channel_t*)dc_vec_at(&ws->welcome_channels, i);
        if (channel) dc_guild_welcome_channel_free(channel);
    }
    dc_vec_free(&ws->welcome_channels);
    memset(ws, 0, sizeof(*ws));
}

static dc_status_t dc_guild_incidents_data_init(dc_guild_incidents_data_t* incidents) {
    if (!incidents) return DC_ERROR_NULL_POINTER;
    memset(incidents, 0, sizeof(*incidents));
    dc_status_t st = dc_nullable_string_init(&incidents->invites_disabled_until);
    if (st != DC_OK) return st;
    st = dc_nullable_string_init(&incidents->dms_disabled_until);
    if (st != DC_OK) return st;
    st = dc_nullable_string_init(&incidents->dm_spam_detected_at);
    if (st != DC_OK) return st;
    st = dc_nullable_string_init(&incidents->raid_detected_at);
    if (st != DC_OK) return st;
    return DC_OK;
}

static void dc_guild_incidents_data_free(dc_guild_incidents_data_t* incidents) {
    if (!incidents) return;
    dc_nullable_string_free(&incidents->invites_disabled_until);
    dc_nullable_string_free(&incidents->dms_disabled_until);
    dc_nullable_string_free(&incidents->dm_spam_detected_at);
    dc_nullable_string_free(&incidents->raid_detected_at);
    memset(incidents, 0, sizeof(*incidents));
}

static dc_status_t dc_guild_get_nullable_snowflake(yyjson_val* obj,
                                                   const char* key,
                                                   dc_nullable_snowflake_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        dc_nullable_snowflake_set_null(out);
        return DC_OK;
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;

    const char* str = yyjson_get_str(field);
    if (!str) return DC_ERROR_INVALID_FORMAT;
    dc_status_t st = dc_snowflake_from_string(str, &out->value);
    if (st != DC_OK) return st;
    out->is_null = 0;
    return DC_OK;
}

static dc_status_t dc_guild_parse_emojis(yyjson_val* obj, int* has_emojis, dc_guild_emoji_list_t* emojis) {
    if (!obj || !has_emojis || !emojis) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, "emojis");
    if (!field || yyjson_is_null(field)) {
        *has_emojis = 0;
        return DC_OK;
    }
    if (!yyjson_is_arr(field)) return DC_ERROR_INVALID_FORMAT;

    *has_emojis = 1;
    size_t idx = 0, max = 0;
    yyjson_val* item = NULL;
    yyjson_arr_foreach(field, idx, max, item) {
        if (!yyjson_is_obj(item)) return DC_ERROR_INVALID_FORMAT;

        dc_guild_emoji_t emoji;
        dc_status_t st = dc_guild_emoji_init(&emoji);
        if (st != DC_OK) return st;

        st = dc_guild_get_optional_snowflake(item, "id", &emoji.id);
        if (st != DC_OK) goto emoji_fail;
        st = dc_guild_get_nullable_string(item, "name", &emoji.name);
        if (st != DC_OK) goto emoji_fail;
        st = dc_guild_get_optional_bool(item, "require_colons", &emoji.require_colons);
        if (st != DC_OK) goto emoji_fail;
        st = dc_guild_get_optional_bool(item, "managed", &emoji.managed);
        if (st != DC_OK) goto emoji_fail;
        st = dc_guild_get_optional_bool(item, "animated", &emoji.animated);
        if (st != DC_OK) goto emoji_fail;
        st = dc_guild_get_optional_bool(item, "available", &emoji.available);
        if (st != DC_OK) goto emoji_fail;

        yyjson_val* roles_val = yyjson_obj_get(item, "roles");
        if (roles_val && !yyjson_is_null(roles_val)) {
            if (!yyjson_is_arr(roles_val)) {
                st = DC_ERROR_INVALID_FORMAT;
                goto emoji_fail;
            }
            size_t ridx = 0, rmax = 0;
            yyjson_val* role_val = NULL;
            yyjson_arr_foreach(roles_val, ridx, rmax, role_val) {
                if (!yyjson_is_str(role_val)) {
                    st = DC_ERROR_INVALID_FORMAT;
                    goto emoji_fail;
                }
                dc_snowflake_t role_id = 0;
                st = dc_snowflake_from_string(yyjson_get_str(role_val), &role_id);
                if (st != DC_OK) goto emoji_fail;
                st = dc_vec_push(&emoji.roles, &role_id);
                if (st != DC_OK) goto emoji_fail;
            }
        }

        yyjson_val* user_val = yyjson_obj_get(item, "user");
        if (user_val && !yyjson_is_null(user_val)) {
            if (!yyjson_is_obj(user_val)) {
                st = DC_ERROR_INVALID_FORMAT;
                goto emoji_fail;
            }
            st = dc_json_model_user_from_val(user_val, &emoji.user);
            if (st != DC_OK) goto emoji_fail;
            emoji.has_user = 1;
        }

        st = dc_vec_push(&emojis->items, &emoji);
        if (st != DC_OK) goto emoji_fail;
        continue;

emoji_fail:
        dc_guild_emoji_free(&emoji);
        return st;
    }

    return DC_OK;
}

static dc_status_t dc_guild_parse_stickers(yyjson_val* obj,
                                           int* has_stickers,
                                           dc_guild_sticker_list_t* stickers) {
    if (!obj || !has_stickers || !stickers) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, "stickers");
    if (!field || yyjson_is_null(field)) {
        *has_stickers = 0;
        return DC_OK;
    }
    if (!yyjson_is_arr(field)) return DC_ERROR_INVALID_FORMAT;

    *has_stickers = 1;
    size_t idx = 0, max = 0;
    yyjson_val* item = NULL;
    yyjson_arr_foreach(field, idx, max, item) {
        if (!yyjson_is_obj(item)) return DC_ERROR_INVALID_FORMAT;

        dc_guild_sticker_t sticker;
        dc_status_t st = dc_guild_sticker_init(&sticker);
        if (st != DC_OK) return st;

        uint64_t id = 0;
        st = dc_json_get_snowflake(item, "id", &id);
        if (st != DC_OK) goto sticker_fail;
        sticker.id = (dc_snowflake_t)id;

        const char* name = NULL;
        st = dc_json_get_string(item, "name", &name);
        if (st != DC_OK) goto sticker_fail;
        st = dc_guild_copy_cstr(&sticker.name, name);
        if (st != DC_OK) goto sticker_fail;

        st = dc_guild_get_nullable_string(item, "description", &sticker.description);
        if (st != DC_OK) goto sticker_fail;

        const char* tags = "";
        st = dc_json_get_string_opt(item, "tags", &tags, "");
        if (st != DC_OK) goto sticker_fail;
        st = dc_guild_copy_cstr(&sticker.tags, tags);
        if (st != DC_OK) goto sticker_fail;

        int64_t i64 = 0;
        st = dc_json_get_int64_opt(item, "type", &i64, (int64_t)0);
        if (st != DC_OK) goto sticker_fail;
        st = dc_guild_i64_to_int_checked(i64, &sticker.type);
        if (st != DC_OK) goto sticker_fail;

        st = dc_json_get_int64_opt(item, "format_type", &i64, (int64_t)0);
        if (st != DC_OK) goto sticker_fail;
        st = dc_guild_i64_to_int_checked(i64, &sticker.format_type);
        if (st != DC_OK) goto sticker_fail;

        st = dc_guild_get_optional_snowflake(item, "pack_id", &sticker.pack_id);
        if (st != DC_OK) goto sticker_fail;
        st = dc_guild_get_optional_snowflake(item, "guild_id", &sticker.guild_id);
        if (st != DC_OK) goto sticker_fail;
        st = dc_guild_get_optional_bool(item, "available", &sticker.available);
        if (st != DC_OK) goto sticker_fail;
        st = dc_guild_get_optional_i32(item, "sort_value", &sticker.sort_value);
        if (st != DC_OK) goto sticker_fail;

        yyjson_val* user_val = yyjson_obj_get(item, "user");
        if (user_val && !yyjson_is_null(user_val)) {
            if (!yyjson_is_obj(user_val)) {
                st = DC_ERROR_INVALID_FORMAT;
                goto sticker_fail;
            }
            st = dc_json_model_user_from_val(user_val, &sticker.user);
            if (st != DC_OK) goto sticker_fail;
            sticker.has_user = 1;
        }

        st = dc_vec_push(&stickers->items, &sticker);
        if (st != DC_OK) goto sticker_fail;
        continue;

sticker_fail:
        dc_guild_sticker_free(&sticker);
        return st;
    }

    return DC_OK;
}

static dc_status_t dc_guild_parse_welcome_screen(yyjson_val* obj,
                                                 int* has_welcome_screen,
                                                 dc_guild_welcome_screen_t* welcome_screen) {
    if (!obj || !has_welcome_screen || !welcome_screen) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, "welcome_screen");
    if (!field || yyjson_is_null(field)) {
        *has_welcome_screen = 0;
        return DC_OK;
    }
    if (!yyjson_is_obj(field)) return DC_ERROR_INVALID_FORMAT;

    *has_welcome_screen = 1;
    dc_status_t st = dc_guild_get_nullable_string(field, "description", &welcome_screen->description);
    if (st != DC_OK) return st;

    yyjson_val* channels = yyjson_obj_get(field, "welcome_channels");
    if (!channels || yyjson_is_null(channels)) return DC_OK;
    if (!yyjson_is_arr(channels)) return DC_ERROR_INVALID_FORMAT;

    size_t idx = 0, max = 0;
    yyjson_val* channel_val = NULL;
    yyjson_arr_foreach(channels, idx, max, channel_val) {
        if (!yyjson_is_obj(channel_val)) return DC_ERROR_INVALID_FORMAT;

        dc_guild_welcome_channel_t channel;
        st = dc_guild_welcome_channel_init(&channel);
        if (st != DC_OK) return st;

        uint64_t channel_id = 0;
        st = dc_json_get_snowflake(channel_val, "channel_id", &channel_id);
        if (st != DC_OK) goto welcome_channel_fail;
        channel.channel_id = (dc_snowflake_t)channel_id;

        const char* description = NULL;
        st = dc_json_get_string(channel_val, "description", &description);
        if (st != DC_OK) goto welcome_channel_fail;
        st = dc_guild_copy_cstr(&channel.description, description);
        if (st != DC_OK) goto welcome_channel_fail;

        st = dc_guild_get_nullable_snowflake(channel_val, "emoji_id", &channel.emoji_id);
        if (st != DC_OK) goto welcome_channel_fail;
        st = dc_guild_get_nullable_string(channel_val, "emoji_name", &channel.emoji_name);
        if (st != DC_OK) goto welcome_channel_fail;

        st = dc_vec_push(&welcome_screen->welcome_channels, &channel);
        if (st != DC_OK) goto welcome_channel_fail;
        continue;

welcome_channel_fail:
        dc_guild_welcome_channel_free(&channel);
        return st;
    }

    return DC_OK;
}

static dc_status_t dc_guild_parse_incidents_data(yyjson_val* obj,
                                                 int* has_incidents_data,
                                                 dc_guild_incidents_data_t* incidents_data) {
    if (!obj || !has_incidents_data || !incidents_data) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, "incidents_data");
    if (!field || yyjson_is_null(field)) {
        *has_incidents_data = 0;
        return DC_OK;
    }
    if (!yyjson_is_obj(field)) return DC_ERROR_INVALID_FORMAT;

    *has_incidents_data = 1;
    dc_status_t st = dc_guild_get_nullable_string(field, "invites_disabled_until",
                                                  &incidents_data->invites_disabled_until);
    if (st != DC_OK) return st;
    st = dc_guild_get_nullable_string(field, "dms_disabled_until",
                                      &incidents_data->dms_disabled_until);
    if (st != DC_OK) return st;
    st = dc_guild_get_nullable_string(field, "dm_spam_detected_at",
                                      &incidents_data->dm_spam_detected_at);
    if (st != DC_OK) return st;
    st = dc_guild_get_nullable_string(field, "raid_detected_at",
                                      &incidents_data->raid_detected_at);
    if (st != DC_OK) return st;
    return DC_OK;
}

static dc_status_t dc_guild_capture_optional_raw_json(yyjson_val* obj,
                                                      const char* key,
                                                      int require_array,
                                                      int require_object,
                                                      int* has_field,
                                                      dc_string_t* out) {
    if (!obj || !key || !has_field || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        *has_field = 0;
        return dc_string_clear(out);
    }

    if (require_array && !yyjson_is_arr(field)) return DC_ERROR_INVALID_FORMAT;
    if (require_object && !yyjson_is_obj(field)) return DC_ERROR_INVALID_FORMAT;

    dc_status_t st = dc_guild_copy_raw_json(field, out);
    if (st != DC_OK) return st;

    *has_field = 1;
    return DC_OK;
}

static dc_status_t dc_guild_get_nullable_string(yyjson_val* obj,
                                                const char* key,
                                                dc_nullable_string_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_null = 1;
        return dc_string_clear(&out->value);
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_null = 0;
    return dc_guild_copy_cstr(&out->value, yyjson_get_str(field));
}

static dc_status_t dc_guild_get_optional_bool(yyjson_val* obj,
                                              const char* key,
                                              dc_optional_bool_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_bool(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_set = 1;
    out->value = yyjson_get_bool(field) ? 1 : 0;
    return DC_OK;
}

static dc_status_t dc_guild_get_optional_i32(yyjson_val* obj,
                                             const char* key,
                                             dc_optional_i32_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_int(field)) return DC_ERROR_INVALID_FORMAT;

    int32_t parsed = 0;
    dc_status_t st = dc_guild_i64_to_i32_checked(yyjson_get_sint(field), &parsed);
    if (st != DC_OK) return st;

    out->is_set = 1;
    out->value = parsed;
    return DC_OK;
}

static dc_status_t dc_guild_get_optional_snowflake(yyjson_val* obj,
                                                   const char* key,
                                                   dc_optional_snowflake_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;

    const char* str = yyjson_get_str(field);
    if (!str) return DC_ERROR_INVALID_FORMAT;

    dc_snowflake_t snowflake = 0;
    dc_status_t st = dc_snowflake_from_string(str, &snowflake);
    if (st != DC_OK) return st;

    out->is_set = 1;
    out->value = snowflake;
    return DC_OK;
}

static dc_status_t dc_guild_get_optional_permission(yyjson_val* obj,
                                                    const char* key,
                                                    dc_optional_u64_field_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;

    uint64_t perm = 0;
    dc_status_t st = dc_json_get_permission(obj, key, &perm);
    if (st != DC_OK) return st;

    out->is_set = 1;
    out->value = perm;
    return DC_OK;
}

static dc_status_t dc_guild_add_nullable_string(dc_json_mut_doc_t* doc,
                                                yyjson_mut_val* obj,
                                                const char* key,
                                                const dc_nullable_string_t* val) {
    if (!doc || !obj || !key || !val) return DC_ERROR_NULL_POINTER;
    if (val->is_null) return dc_json_mut_set_null(doc, obj, key);
    return dc_json_mut_set_string(doc, obj, key, dc_string_cstr(&val->value));
}

static dc_status_t dc_guild_add_optional_bool(dc_json_mut_doc_t* doc,
                                              yyjson_mut_val* obj,
                                              const char* key,
                                              const dc_optional_bool_t* val) {
    if (!doc || !obj || !key || !val) return DC_ERROR_NULL_POINTER;
    if (!val->is_set) return DC_OK;
    return dc_json_mut_set_bool(doc, obj, key, val->value);
}

static dc_status_t dc_guild_add_optional_i32(dc_json_mut_doc_t* doc,
                                             yyjson_mut_val* obj,
                                             const char* key,
                                             const dc_optional_i32_t* val) {
    if (!doc || !obj || !key || !val) return DC_ERROR_NULL_POINTER;
    if (!val->is_set) return DC_OK;
    return dc_json_mut_set_int64(doc, obj, key, (int64_t)val->value);
}

static dc_status_t dc_guild_add_optional_snowflake(dc_json_mut_doc_t* doc,
                                                   yyjson_mut_val* obj,
                                                   const char* key,
                                                   const dc_optional_snowflake_t* val) {
    if (!doc || !obj || !key || !val) return DC_ERROR_NULL_POINTER;
    if (!val->is_set) return DC_OK;
    return dc_json_mut_set_snowflake(doc, obj, key, val->value);
}

static dc_status_t dc_guild_add_optional_permission(dc_json_mut_doc_t* doc,
                                                    yyjson_mut_val* obj,
                                                    const char* key,
                                                    const dc_optional_u64_field_t* val) {
    if (!doc || !obj || !key || !val) return DC_ERROR_NULL_POINTER;
    if (!val->is_set) return DC_OK;
    return dc_json_mut_set_permission(doc, obj, key, val->value);
}

static dc_status_t dc_guild_add_nullable_snowflake(dc_json_mut_doc_t* doc,
                                                   yyjson_mut_val* obj,
                                                   const char* key,
                                                   const dc_nullable_snowflake_t* val) {
    if (!doc || !obj || !key || !val) return DC_ERROR_NULL_POINTER;
    if (val->is_null) return dc_json_mut_set_null(doc, obj, key);
    return dc_json_mut_set_snowflake(doc, obj, key, val->value);
}

static dc_status_t dc_guild_add_roles_if_set(dc_json_mut_doc_t* doc,
                                             yyjson_mut_val* obj,
                                             int has_roles,
                                             const dc_role_list_t* roles) {
    if (!doc || !doc->doc || !obj || !roles) return DC_ERROR_NULL_POINTER;
    if (!has_roles) return DC_OK;

    yyjson_mut_val* roles_arr = yyjson_mut_obj_add_arr(doc->doc, obj, "roles");
    if (!roles_arr) return DC_ERROR_OUT_OF_MEMORY;

    for (size_t i = 0; i < dc_vec_length(&roles->items); i++) {
        const dc_role_t* role = (const dc_role_t*)dc_vec_at(&roles->items, i);
        if (!role) return DC_ERROR_INVALID_FORMAT;

        yyjson_mut_val* role_obj = yyjson_mut_arr_add_obj(doc->doc, roles_arr);
        if (!role_obj) return DC_ERROR_OUT_OF_MEMORY;

        dc_status_t st = dc_json_model_role_to_mut(doc, role_obj, role);
        if (st != DC_OK) return st;
    }

    return DC_OK;
}

static dc_status_t dc_guild_add_emojis_if_set(dc_json_mut_doc_t* doc,
                                              yyjson_mut_val* obj,
                                              int has_emojis,
                                              const dc_guild_emoji_list_t* emojis) {
    if (!doc || !doc->doc || !obj || !emojis) return DC_ERROR_NULL_POINTER;
    if (!has_emojis) return DC_OK;

    yyjson_mut_val* emojis_arr = yyjson_mut_obj_add_arr(doc->doc, obj, "emojis");
    if (!emojis_arr) return DC_ERROR_OUT_OF_MEMORY;

    for (size_t i = 0; i < dc_vec_length(&emojis->items); i++) {
        const dc_guild_emoji_t* emoji = (const dc_guild_emoji_t*)dc_vec_at(&emojis->items, i);
        if (!emoji) return DC_ERROR_INVALID_FORMAT;

        yyjson_mut_val* emoji_obj = yyjson_mut_arr_add_obj(doc->doc, emojis_arr);
        if (!emoji_obj) return DC_ERROR_OUT_OF_MEMORY;

        if (emoji->id.is_set) {
            dc_status_t st = dc_json_mut_set_snowflake(doc, emoji_obj, "id", emoji->id.value);
            if (st != DC_OK) return st;
        }
        dc_status_t st = dc_guild_add_nullable_string(doc, emoji_obj, "name", &emoji->name);
        if (st != DC_OK) return st;

        if (!dc_vec_is_empty(&emoji->roles)) {
            yyjson_mut_val* roles_arr = yyjson_mut_obj_add_arr(doc->doc, emoji_obj, "roles");
            if (!roles_arr) return DC_ERROR_OUT_OF_MEMORY;
            for (size_t r = 0; r < dc_vec_length(&emoji->roles); r++) {
                const dc_snowflake_t* role_id = (const dc_snowflake_t*)dc_vec_at(&emoji->roles, r);
                if (!role_id) return DC_ERROR_INVALID_FORMAT;
                char role_buf[32];
                st = dc_snowflake_to_cstr(*role_id, role_buf, sizeof(role_buf));
                if (st != DC_OK) return st;
                if (!yyjson_mut_arr_add_strcpy(doc->doc, roles_arr, role_buf)) return DC_ERROR_OUT_OF_MEMORY;
            }
        }

        if (emoji->has_user) {
            yyjson_mut_val* user_obj = yyjson_mut_obj_add_obj(doc->doc, emoji_obj, "user");
            if (!user_obj) return DC_ERROR_OUT_OF_MEMORY;
            st = dc_json_model_user_to_mut(doc, user_obj, &emoji->user);
            if (st != DC_OK) return st;
        }

        st = dc_guild_add_optional_bool(doc, emoji_obj, "require_colons", &emoji->require_colons);
        if (st != DC_OK) return st;
        st = dc_guild_add_optional_bool(doc, emoji_obj, "managed", &emoji->managed);
        if (st != DC_OK) return st;
        st = dc_guild_add_optional_bool(doc, emoji_obj, "animated", &emoji->animated);
        if (st != DC_OK) return st;
        st = dc_guild_add_optional_bool(doc, emoji_obj, "available", &emoji->available);
        if (st != DC_OK) return st;
    }

    return DC_OK;
}

static dc_status_t dc_guild_add_stickers_if_set(dc_json_mut_doc_t* doc,
                                                yyjson_mut_val* obj,
                                                int has_stickers,
                                                const dc_guild_sticker_list_t* stickers) {
    if (!doc || !doc->doc || !obj || !stickers) return DC_ERROR_NULL_POINTER;
    if (!has_stickers) return DC_OK;

    yyjson_mut_val* stickers_arr = yyjson_mut_obj_add_arr(doc->doc, obj, "stickers");
    if (!stickers_arr) return DC_ERROR_OUT_OF_MEMORY;

    for (size_t i = 0; i < dc_vec_length(&stickers->items); i++) {
        const dc_guild_sticker_t* sticker = (const dc_guild_sticker_t*)dc_vec_at(&stickers->items, i);
        if (!sticker) return DC_ERROR_INVALID_FORMAT;

        yyjson_mut_val* sticker_obj = yyjson_mut_arr_add_obj(doc->doc, stickers_arr);
        if (!sticker_obj) return DC_ERROR_OUT_OF_MEMORY;

        dc_status_t st = dc_json_mut_set_snowflake(doc, sticker_obj, "id", sticker->id);
        if (st != DC_OK) return st;
        st = dc_json_mut_set_string(doc, sticker_obj, "name", dc_string_cstr(&sticker->name));
        if (st != DC_OK) return st;
        st = dc_guild_add_nullable_string(doc, sticker_obj, "description", &sticker->description);
        if (st != DC_OK) return st;
        st = dc_json_mut_set_string(doc, sticker_obj, "tags", dc_string_cstr(&sticker->tags));
        if (st != DC_OK) return st;
        st = dc_json_mut_set_int64(doc, sticker_obj, "type", (int64_t)sticker->type);
        if (st != DC_OK) return st;
        st = dc_json_mut_set_int64(doc, sticker_obj, "format_type", (int64_t)sticker->format_type);
        if (st != DC_OK) return st;
        st = dc_guild_add_optional_snowflake(doc, sticker_obj, "pack_id", &sticker->pack_id);
        if (st != DC_OK) return st;
        st = dc_guild_add_optional_bool(doc, sticker_obj, "available", &sticker->available);
        if (st != DC_OK) return st;
        st = dc_guild_add_optional_snowflake(doc, sticker_obj, "guild_id", &sticker->guild_id);
        if (st != DC_OK) return st;
        st = dc_guild_add_optional_i32(doc, sticker_obj, "sort_value", &sticker->sort_value);
        if (st != DC_OK) return st;
        if (sticker->has_user) {
            yyjson_mut_val* user_obj = yyjson_mut_obj_add_obj(doc->doc, sticker_obj, "user");
            if (!user_obj) return DC_ERROR_OUT_OF_MEMORY;
            st = dc_json_model_user_to_mut(doc, user_obj, &sticker->user);
            if (st != DC_OK) return st;
        }
    }

    return DC_OK;
}

static dc_status_t dc_guild_add_welcome_screen_if_set(dc_json_mut_doc_t* doc,
                                                      yyjson_mut_val* obj,
                                                      int has_welcome_screen,
                                                      const dc_guild_welcome_screen_t* welcome_screen) {
    if (!doc || !doc->doc || !obj || !welcome_screen) return DC_ERROR_NULL_POINTER;
    if (!has_welcome_screen) return DC_OK;

    yyjson_mut_val* ws_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "welcome_screen");
    if (!ws_obj) return DC_ERROR_OUT_OF_MEMORY;

    dc_status_t st = dc_guild_add_nullable_string(doc, ws_obj, "description", &welcome_screen->description);
    if (st != DC_OK) return st;

    yyjson_mut_val* channels_arr = yyjson_mut_obj_add_arr(doc->doc, ws_obj, "welcome_channels");
    if (!channels_arr) return DC_ERROR_OUT_OF_MEMORY;

    for (size_t i = 0; i < dc_vec_length(&welcome_screen->welcome_channels); i++) {
        const dc_guild_welcome_channel_t* channel =
            (const dc_guild_welcome_channel_t*)dc_vec_at(&welcome_screen->welcome_channels, i);
        if (!channel) return DC_ERROR_INVALID_FORMAT;

        yyjson_mut_val* channel_obj = yyjson_mut_arr_add_obj(doc->doc, channels_arr);
        if (!channel_obj) return DC_ERROR_OUT_OF_MEMORY;

        st = dc_json_mut_set_snowflake(doc, channel_obj, "channel_id", channel->channel_id);
        if (st != DC_OK) return st;
        st = dc_json_mut_set_string(doc, channel_obj, "description", dc_string_cstr(&channel->description));
        if (st != DC_OK) return st;
        st = dc_guild_add_nullable_snowflake(doc, channel_obj, "emoji_id", &channel->emoji_id);
        if (st != DC_OK) return st;
        st = dc_guild_add_nullable_string(doc, channel_obj, "emoji_name", &channel->emoji_name);
        if (st != DC_OK) return st;
    }

    return DC_OK;
}

static dc_status_t dc_guild_add_incidents_data_if_set(dc_json_mut_doc_t* doc,
                                                      yyjson_mut_val* obj,
                                                      int has_incidents_data,
                                                      const dc_guild_incidents_data_t* incidents_data) {
    if (!doc || !doc->doc || !obj || !incidents_data) return DC_ERROR_NULL_POINTER;
    if (!has_incidents_data) return DC_OK;

    yyjson_mut_val* incidents_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "incidents_data");
    if (!incidents_obj) return DC_ERROR_OUT_OF_MEMORY;

    dc_status_t st = dc_guild_add_nullable_string(doc, incidents_obj, "invites_disabled_until",
                                                  &incidents_data->invites_disabled_until);
    if (st != DC_OK) return st;
    st = dc_guild_add_nullable_string(doc, incidents_obj, "dms_disabled_until",
                                      &incidents_data->dms_disabled_until);
    if (st != DC_OK) return st;
    st = dc_guild_add_nullable_string(doc, incidents_obj, "dm_spam_detected_at",
                                      &incidents_data->dm_spam_detected_at);
    if (st != DC_OK) return st;
    st = dc_guild_add_nullable_string(doc, incidents_obj, "raid_detected_at",
                                      &incidents_data->raid_detected_at);
    if (st != DC_OK) return st;
    return DC_OK;
}

dc_status_t dc_json_model_guild_from_val(yyjson_val* val, dc_guild_t* guild) {
    if (!val || !guild) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_status_t st = DC_OK;
    uint64_t id = 0;
    const char* name = NULL;

    st = dc_json_get_snowflake(val, "id", &id);
    if (st != DC_OK) return st;
    st = dc_json_get_string(val, "name", &name);
    if (st != DC_OK) return st;

    guild->id = (dc_snowflake_t)id;
    st = dc_guild_copy_cstr(&guild->name, name);
    if (st != DC_OK) return st;

    st = dc_guild_get_nullable_string(val, "icon", &guild->icon);
    if (st != DC_OK) return st;
    st = dc_guild_get_nullable_string(val, "icon_hash", &guild->icon_hash);
    if (st != DC_OK) return st;
    st = dc_guild_get_nullable_string(val, "splash", &guild->splash);
    if (st != DC_OK) return st;
    st = dc_guild_get_nullable_string(val, "discovery_splash", &guild->discovery_splash);
    if (st != DC_OK) return st;
    st = dc_guild_get_nullable_string(val, "vanity_url_code", &guild->vanity_url_code);
    if (st != DC_OK) return st;
    st = dc_guild_get_nullable_string(val, "description", &guild->description);
    if (st != DC_OK) return st;
    st = dc_guild_get_nullable_string(val, "banner", &guild->banner);
    if (st != DC_OK) return st;
    st = dc_guild_parse_features(val, &guild->has_features, &guild->features);
    if (st != DC_OK) return st;
    st = dc_guild_parse_roles(val, &guild->roles);
    if (st != DC_OK) return st;
    st = dc_guild_parse_emojis(val, &guild->has_emojis, &guild->emojis);
    if (st != DC_OK) return st;
    st = dc_guild_parse_welcome_screen(val, &guild->has_welcome_screen, &guild->welcome_screen);
    if (st != DC_OK) return st;
    st = dc_guild_parse_stickers(val, &guild->has_stickers, &guild->stickers);
    if (st != DC_OK) return st;
    st = dc_guild_parse_incidents_data(val, &guild->has_incidents_data, &guild->incidents_data);
    if (st != DC_OK) return st;

    st = dc_guild_get_optional_bool(val, "owner", &guild->owner);
    if (st != DC_OK) return st;
    st = dc_guild_get_optional_snowflake(val, "owner_id", &guild->owner_id);
    if (st != DC_OK) return st;
    st = dc_guild_get_optional_permission(val, "permissions", &guild->permissions);
    if (st != DC_OK) return st;

    st = dc_guild_get_optional_snowflake(val, "afk_channel_id", &guild->afk_channel_id);
    if (st != DC_OK) return st;

    int64_t i64 = 0;
    st = dc_json_get_int64_opt(val, "afk_timeout", &i64, (int64_t)0);
    if (st != DC_OK) return st;
    st = dc_guild_i64_to_int_checked(i64, &guild->afk_timeout);
    if (st != DC_OK) return st;

    st = dc_guild_get_optional_bool(val, "widget_enabled", &guild->widget_enabled);
    if (st != DC_OK) return st;
    st = dc_guild_get_optional_snowflake(val, "widget_channel_id", &guild->widget_channel_id);
    if (st != DC_OK) return st;

    st = dc_json_get_int64_opt(val, "verification_level", &i64, (int64_t)0);
    if (st != DC_OK) return st;
    st = dc_guild_i64_to_int_checked(i64, &guild->verification_level);
    if (st != DC_OK) return st;

    st = dc_json_get_int64_opt(val, "default_message_notifications", &i64, (int64_t)0);
    if (st != DC_OK) return st;
    st = dc_guild_i64_to_int_checked(i64, &guild->default_message_notifications);
    if (st != DC_OK) return st;

    st = dc_json_get_int64_opt(val, "explicit_content_filter", &i64, (int64_t)0);
    if (st != DC_OK) return st;
    st = dc_guild_i64_to_int_checked(i64, &guild->explicit_content_filter);
    if (st != DC_OK) return st;

    st = dc_json_get_int64_opt(val, "mfa_level", &i64, (int64_t)0);
    if (st != DC_OK) return st;
    st = dc_guild_i64_to_int_checked(i64, &guild->mfa_level);
    if (st != DC_OK) return st;

    st = dc_guild_get_optional_snowflake(val, "application_id", &guild->application_id);
    if (st != DC_OK) return st;
    st = dc_guild_get_optional_snowflake(val, "system_channel_id", &guild->system_channel_id);
    if (st != DC_OK) return st;

    st = dc_json_get_int64_opt(val, "system_channel_flags", &i64, (int64_t)0);
    if (st != DC_OK) return st;
    if (i64 < 0) return DC_ERROR_INVALID_FORMAT;
    guild->system_channel_flags = (uint64_t)i64;

    st = dc_guild_get_optional_snowflake(val, "rules_channel_id", &guild->rules_channel_id);
    if (st != DC_OK) return st;

    st = dc_guild_get_optional_i32(val, "max_presences", &guild->max_presences);
    if (st != DC_OK) return st;
    st = dc_guild_get_optional_i32(val, "max_members", &guild->max_members);
    if (st != DC_OK) return st;

    st = dc_json_get_int64_opt(val, "premium_tier", &i64, (int64_t)0);
    if (st != DC_OK) return st;
    st = dc_guild_i64_to_int_checked(i64, &guild->premium_tier);
    if (st != DC_OK) return st;

    st = dc_guild_get_optional_i32(val, "premium_subscription_count", &guild->premium_subscription_count);
    if (st != DC_OK) return st;

    const char* preferred_locale = "en-US";
    st = dc_json_get_string_opt(val, "preferred_locale", &preferred_locale, "en-US");
    if (st != DC_OK) return st;
    st = dc_guild_copy_cstr(&guild->preferred_locale, preferred_locale);
    if (st != DC_OK) return st;

    st = dc_guild_get_optional_snowflake(val, "public_updates_channel_id", &guild->public_updates_channel_id);
    if (st != DC_OK) return st;
    st = dc_guild_get_optional_i32(val, "max_video_channel_users", &guild->max_video_channel_users);
    if (st != DC_OK) return st;
    st = dc_guild_get_optional_i32(val, "max_stage_video_channel_users", &guild->max_stage_video_channel_users);
    if (st != DC_OK) return st;
    st = dc_guild_get_optional_i32(val, "approximate_member_count", &guild->approximate_member_count);
    if (st != DC_OK) return st;
    st = dc_guild_get_optional_i32(val, "approximate_presence_count", &guild->approximate_presence_count);
    if (st != DC_OK) return st;

    st = dc_json_get_int64_opt(val, "nsfw_level", &i64, (int64_t)0);
    if (st != DC_OK) return st;
    st = dc_guild_i64_to_int_checked(i64, &guild->nsfw_level);
    if (st != DC_OK) return st;

    int progress = 0;
    st = dc_json_get_bool_opt(val, "premium_progress_bar_enabled", &progress, 0);
    if (st != DC_OK) return st;
    guild->premium_progress_bar_enabled = progress;

    st = dc_guild_get_optional_snowflake(val, "safety_alerts_channel_id", &guild->safety_alerts_channel_id);
    if (st != DC_OK) return st;
    st = dc_guild_capture_optional_raw_json(val, "roles", 1, 0, &guild->has_roles, &guild->roles_json);
    if (st != DC_OK) return st;
    st = dc_guild_capture_optional_raw_json(val, "emojis", 1, 0, &guild->has_emojis, &guild->emojis_json);
    if (st != DC_OK) return st;
    st = dc_guild_capture_optional_raw_json(val, "welcome_screen", 0, 1,
                                            &guild->has_welcome_screen, &guild->welcome_screen_json);
    if (st != DC_OK) return st;
    st = dc_guild_capture_optional_raw_json(val, "stickers", 1, 0,
                                            &guild->has_stickers, &guild->stickers_json);
    if (st != DC_OK) return st;
    st = dc_guild_capture_optional_raw_json(val, "incidents_data", 0, 1,
                                            &guild->has_incidents_data, &guild->incidents_data_json);
    if (st != DC_OK) return st;

    return DC_OK;
}

static dc_status_t dc_guild_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const dc_guild_t* guild) {
    if (!doc || !doc->doc || !obj || !guild) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    dc_status_t st = dc_json_mut_set_snowflake(doc, obj, "id", guild->id);
    if (st != DC_OK) return st;

    st = dc_json_mut_set_string(doc, obj, "name", dc_string_cstr(&guild->name));
    if (st != DC_OK) return st;

    st = dc_guild_add_nullable_string(doc, obj, "icon", &guild->icon);
    if (st != DC_OK) return st;
    st = dc_guild_add_nullable_string(doc, obj, "icon_hash", &guild->icon_hash);
    if (st != DC_OK) return st;
    st = dc_guild_add_nullable_string(doc, obj, "splash", &guild->splash);
    if (st != DC_OK) return st;
    st = dc_guild_add_nullable_string(doc, obj, "discovery_splash", &guild->discovery_splash);
    if (st != DC_OK) return st;

    st = dc_guild_add_optional_bool(doc, obj, "owner", &guild->owner);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_snowflake(doc, obj, "owner_id", &guild->owner_id);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_permission(doc, obj, "permissions", &guild->permissions);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_snowflake(doc, obj, "afk_channel_id", &guild->afk_channel_id);
    if (st != DC_OK) return st;

    st = dc_json_mut_set_int64(doc, obj, "afk_timeout", (int64_t)guild->afk_timeout);
    if (st != DC_OK) return st;

    st = dc_guild_add_optional_bool(doc, obj, "widget_enabled", &guild->widget_enabled);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_snowflake(doc, obj, "widget_channel_id", &guild->widget_channel_id);
    if (st != DC_OK) return st;

    st = dc_json_mut_set_int64(doc, obj, "verification_level", (int64_t)guild->verification_level);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_int64(doc, obj, "default_message_notifications",
                               (int64_t)guild->default_message_notifications);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_int64(doc, obj, "explicit_content_filter", (int64_t)guild->explicit_content_filter);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_int64(doc, obj, "mfa_level", (int64_t)guild->mfa_level);
    if (st != DC_OK) return st;

    st = dc_guild_add_optional_snowflake(doc, obj, "application_id", &guild->application_id);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_snowflake(doc, obj, "system_channel_id", &guild->system_channel_id);
    if (st != DC_OK) return st;

    int64_t system_flags_i64 = 0;
    st = dc_guild_u64_to_i64_checked(guild->system_channel_flags, &system_flags_i64);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_int64(doc, obj, "system_channel_flags", system_flags_i64);
    if (st != DC_OK) return st;

    st = dc_guild_add_optional_snowflake(doc, obj, "rules_channel_id", &guild->rules_channel_id);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_i32(doc, obj, "max_presences", &guild->max_presences);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_i32(doc, obj, "max_members", &guild->max_members);
    if (st != DC_OK) return st;

    st = dc_guild_add_nullable_string(doc, obj, "vanity_url_code", &guild->vanity_url_code);
    if (st != DC_OK) return st;
    st = dc_guild_add_nullable_string(doc, obj, "description", &guild->description);
    if (st != DC_OK) return st;
    st = dc_guild_add_nullable_string(doc, obj, "banner", &guild->banner);
    if (st != DC_OK) return st;
    if (guild->has_features) {
        yyjson_mut_val* features_arr = yyjson_mut_obj_add_arr(doc->doc, obj, "features");
        if (!features_arr) return DC_ERROR_OUT_OF_MEMORY;

        for (size_t i = 0; i < dc_vec_length(&guild->features); i++) {
            const dc_string_t* feature = (const dc_string_t*)dc_vec_at(&guild->features, i);
            if (!feature) return DC_ERROR_INVALID_FORMAT;

            yyjson_mut_val* feature_val = yyjson_mut_strcpy(doc->doc, dc_string_cstr(feature));
            if (!feature_val) return DC_ERROR_OUT_OF_MEMORY;
            if (!yyjson_mut_arr_add_val(features_arr, feature_val)) return DC_ERROR_OUT_OF_MEMORY;
        }
    }

    st = dc_json_mut_set_int64(doc, obj, "premium_tier", (int64_t)guild->premium_tier);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_i32(doc, obj, "premium_subscription_count", &guild->premium_subscription_count);
    if (st != DC_OK) return st;

    st = dc_json_mut_set_string(doc, obj, "preferred_locale", dc_string_cstr(&guild->preferred_locale));
    if (st != DC_OK) return st;

    st = dc_guild_add_optional_snowflake(doc, obj, "public_updates_channel_id",
                                         &guild->public_updates_channel_id);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_i32(doc, obj, "max_video_channel_users", &guild->max_video_channel_users);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_i32(doc, obj, "max_stage_video_channel_users",
                                   &guild->max_stage_video_channel_users);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_i32(doc, obj, "approximate_member_count", &guild->approximate_member_count);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_i32(doc, obj, "approximate_presence_count", &guild->approximate_presence_count);
    if (st != DC_OK) return st;

    st = dc_json_mut_set_int64(doc, obj, "nsfw_level", (int64_t)guild->nsfw_level);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_bool(doc, obj, "premium_progress_bar_enabled",
                              guild->premium_progress_bar_enabled);
    if (st != DC_OK) return st;
    st = dc_guild_add_optional_snowflake(doc, obj, "safety_alerts_channel_id",
                                         &guild->safety_alerts_channel_id);
    if (st != DC_OK) return st;
    st = dc_guild_add_roles_if_set(doc, obj, guild->has_roles, &guild->roles);
    if (st != DC_OK) return st;
    st = dc_guild_add_emojis_if_set(doc, obj, guild->has_emojis, &guild->emojis);
    if (st != DC_OK) return st;
    st = dc_guild_add_welcome_screen_if_set(doc, obj,
                                            guild->has_welcome_screen, &guild->welcome_screen);
    if (st != DC_OK) return st;
    st = dc_guild_add_stickers_if_set(doc, obj, guild->has_stickers, &guild->stickers);
    if (st != DC_OK) return st;
    st = dc_guild_add_incidents_data_if_set(doc, obj,
                                            guild->has_incidents_data, &guild->incidents_data);
    if (st != DC_OK) return st;

    return DC_OK;
}

dc_status_t dc_guild_init(dc_guild_t* guild) {
    if (!guild) return DC_ERROR_NULL_POINTER;
    memset(guild, 0, sizeof(*guild));

    dc_status_t st = dc_string_init(&guild->name);
    if (st != DC_OK) return st;

    st = dc_nullable_string_init(&guild->icon);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&guild->icon_hash);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&guild->splash);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&guild->discovery_splash);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&guild->vanity_url_code);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&guild->description);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&guild->banner);
    if (st != DC_OK) goto fail;
    st = dc_string_init(&guild->preferred_locale);
    if (st != DC_OK) goto fail;
    st = dc_role_list_init(&guild->roles);
    if (st != DC_OK) goto fail;
    st = dc_vec_init(&guild->features, sizeof(dc_string_t));
    if (st != DC_OK) goto fail;
    st = dc_guild_emoji_list_init(&guild->emojis);
    if (st != DC_OK) goto fail;
    st = dc_guild_welcome_screen_init(&guild->welcome_screen);
    if (st != DC_OK) goto fail;
    st = dc_guild_sticker_list_init(&guild->stickers);
    if (st != DC_OK) goto fail;
    st = dc_guild_incidents_data_init(&guild->incidents_data);
    if (st != DC_OK) goto fail;
    st = dc_string_init(&guild->roles_json);
    if (st != DC_OK) goto fail;
    st = dc_string_init(&guild->emojis_json);
    if (st != DC_OK) goto fail;
    st = dc_string_init(&guild->welcome_screen_json);
    if (st != DC_OK) goto fail;
    st = dc_string_init(&guild->stickers_json);
    if (st != DC_OK) goto fail;
    st = dc_string_init(&guild->incidents_data_json);
    if (st != DC_OK) goto fail;

    return DC_OK;

fail:
    dc_guild_free(guild);
    return st;
}

void dc_guild_free(dc_guild_t* guild) {
    if (!guild) return;
    dc_string_free(&guild->name);
    dc_nullable_string_free(&guild->icon);
    dc_nullable_string_free(&guild->icon_hash);
    dc_nullable_string_free(&guild->splash);
    dc_nullable_string_free(&guild->discovery_splash);
    dc_nullable_string_free(&guild->vanity_url_code);
    dc_nullable_string_free(&guild->description);
    dc_nullable_string_free(&guild->banner);
    dc_string_free(&guild->preferred_locale);
    dc_role_list_free(&guild->roles);
    for (size_t i = 0; i < dc_vec_length(&guild->features); i++) {
        dc_string_t* feature = (dc_string_t*)dc_vec_at(&guild->features, i);
        if (feature) dc_string_free(feature);
    }
    dc_vec_free(&guild->features);
    dc_guild_emoji_list_free(&guild->emojis);
    dc_guild_welcome_screen_free(&guild->welcome_screen);
    dc_guild_sticker_list_free(&guild->stickers);
    dc_guild_incidents_data_free(&guild->incidents_data);
    dc_string_free(&guild->roles_json);
    dc_string_free(&guild->emojis_json);
    dc_string_free(&guild->welcome_screen_json);
    dc_string_free(&guild->stickers_json);
    dc_string_free(&guild->incidents_data_json);
    memset(guild, 0, sizeof(*guild));
}

dc_status_t dc_guild_from_json(const char* json_data, dc_guild_t* guild) {
    if (!json_data || !guild) return DC_ERROR_NULL_POINTER;

    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(json_data, &doc);
    if (st != DC_OK) return st;

    dc_guild_t tmp;
    st = dc_guild_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    st = dc_json_model_guild_from_val(doc.root, &tmp);
    dc_json_doc_free(&doc);
    if (st != DC_OK) {
        dc_guild_free(&tmp);
        return st;
    }

    *guild = tmp;
    return DC_OK;
}

dc_status_t dc_guild_to_json(const dc_guild_t* guild, dc_string_t* json_result) {
    if (!guild || !json_result) return DC_ERROR_NULL_POINTER;

    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;

    st = dc_guild_to_mut(&doc, doc.root, guild);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }

    st = dc_json_mut_doc_serialize(&doc, json_result);
    dc_json_mut_doc_free(&doc);
    return st;
}
