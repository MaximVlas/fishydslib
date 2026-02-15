/**
 * @file dc_json_model.c
 * @brief JSON model parsing/building helpers
 */

#include "dc_json_model.h"
#include "core/dc_time.h"
#include "core/dc_alloc.h"
#include "model/dc_user.h"
#include <limits.h>
#include <inttypes.h>
#include <string.h>
#include <yyjson.h>

static dc_status_t dc_json_copy_cstr(dc_string_t* dst, const char* src) {
    if (!dst) return DC_ERROR_NULL_POINTER;
    return dc_string_set_cstr(dst, src ? src : "");
}

static dc_status_t dc_json_parse_iso8601_if_set(const char* str) {
    if (!str || str[0] == '\0') return DC_OK;
    dc_iso8601_t ts;
    return dc_iso8601_parse(str, &ts);
}

static dc_status_t dc_int64_to_int_checked(int64_t val, int* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (val < INT_MIN || val > INT_MAX) return DC_ERROR_INVALID_FORMAT;
    *out = (int)val;
    return DC_OK;
}

static dc_status_t dc_int64_to_u32_checked(int64_t val, uint32_t* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (val < 0 || val > (int64_t)UINT32_MAX) return DC_ERROR_INVALID_FORMAT;
    *out = (uint32_t)val;
    return DC_OK;
}

static dc_status_t dc_json_get_snowflake_optional_field(yyjson_val* obj, const char* key,
                                                        dc_optional_snowflake_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;
    const char* str = yyjson_get_str(field);
    if (!str) return DC_ERROR_INVALID_FORMAT;
    dc_snowflake_t sf = 0;
    dc_status_t st = dc_snowflake_from_string(str, &sf);
    if (st != DC_OK) return st;
    out->is_set = 1;
    out->value = sf;
    return DC_OK;
}

static dc_status_t dc_json_get_permission_optional_field(yyjson_val* obj, const char* key,
                                                         dc_optional_u64_field_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;
    const char* str = yyjson_get_str(field);
    if (!str) return DC_ERROR_INVALID_FORMAT;
    uint64_t val = 0;
    dc_status_t st = dc_json_get_permission(obj, key, &val);
    if (st != DC_OK) return st;
    out->is_set = 1;
    out->value = val;
    return DC_OK;
}

static dc_status_t dc_json_get_nullable_string_field(yyjson_val* obj, const char* key,
                                                     dc_nullable_string_t* out, int treat_missing_as_null) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field) {
        if (treat_missing_as_null) {
            out->is_null = 1;
            return dc_string_clear(&out->value);
        }
        return DC_ERROR_NOT_FOUND;
    }
    if (yyjson_is_null(field)) {
        out->is_null = 1;
        return dc_string_clear(&out->value);
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;
    out->is_null = 0;
    return dc_json_copy_cstr(&out->value, yyjson_get_str(field));
}

static dc_status_t dc_json_get_nullable_snowflake_field(yyjson_val* obj, const char* key,
                                                        dc_nullable_snowflake_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_null = 1;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;
    const char* str = yyjson_get_str(field);
    if (!str) return DC_ERROR_INVALID_FORMAT;
    dc_snowflake_t sf = 0;
    dc_status_t st = dc_snowflake_from_string(str, &sf);
    if (st != DC_OK) return st;
    out->is_null = 0;
    out->value = sf;
    return DC_OK;
}

static dc_status_t dc_json_get_nullable_bool_field(yyjson_val* obj, const char* key,
                                                   dc_nullable_bool_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_null = 1;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_bool(field)) return DC_ERROR_INVALID_FORMAT;
    out->is_null = 0;
    out->value = yyjson_get_bool(field) ? 1 : 0;
    return DC_OK;
}

static dc_status_t dc_json_parse_snowflake_array(yyjson_val* arr, dc_vec_t* out) {
    if (!arr || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_arr(arr)) return DC_ERROR_INVALID_FORMAT;
    yyjson_arr_iter iter = yyjson_arr_iter_with(arr);
    yyjson_val* val = NULL;
    while ((val = yyjson_arr_iter_next(&iter))) {
        if (!yyjson_is_str(val)) return DC_ERROR_INVALID_FORMAT;
        const char* str = yyjson_get_str(val);
        if (!str) return DC_ERROR_INVALID_FORMAT;
        dc_snowflake_t sf = 0;
        dc_status_t st = dc_snowflake_from_string(str, &sf);
        if (st != DC_OK) return st;
        st = dc_vec_push(out, &sf);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

static dc_status_t dc_json_parse_permission_overwrites(yyjson_val* arr, dc_vec_t* out) {
    if (!arr || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_arr(arr)) return DC_ERROR_INVALID_FORMAT;

    yyjson_arr_iter iter = yyjson_arr_iter_with(arr);
    yyjson_val* val = NULL;
    while ((val = yyjson_arr_iter_next(&iter))) {
        if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

        dc_permission_overwrite_t ow;
        memset(&ow, 0, sizeof(ow));

        dc_status_t st = dc_json_get_snowflake(val, "id", &ow.id);
        if (st != DC_OK) return st;

        int64_t type_i64 = 0;
        st = dc_json_get_int64(val, "type", &type_i64);
        if (st != DC_OK) return st;
        int type_int = 0;
        st = dc_int64_to_int_checked(type_i64, &type_int);
        if (st != DC_OK) return st;
        if (type_int != 0 && type_int != 1) return DC_ERROR_INVALID_FORMAT;
        ow.type = (dc_permission_overwrite_type_t)type_int;

        uint64_t allow = 0;
        st = dc_json_get_permission_opt(val, "allow", &allow, 0ULL);
        if (st != DC_OK) return st;
        uint64_t deny = 0;
        st = dc_json_get_permission_opt(val, "deny", &deny, 0ULL);
        if (st != DC_OK) return st;
        ow.allow = allow;
        ow.deny = deny;

        st = dc_vec_push(out, &ow);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

static dc_status_t dc_channel_forum_tag_init(dc_channel_forum_tag_t* tag) {
    if (!tag) return DC_ERROR_NULL_POINTER;
    memset(tag, 0, sizeof(*tag));
    dc_status_t st = dc_string_init(&tag->name);
    if (st != DC_OK) return st;
    st = dc_optional_string_init(&tag->emoji_name);
    return st;
}

static void dc_channel_forum_tag_free(dc_channel_forum_tag_t* tag) {
    if (!tag) return;
    dc_string_free(&tag->name);
    dc_optional_string_free(&tag->emoji_name);
    memset(tag, 0, sizeof(*tag));
}

static dc_status_t dc_json_parse_forum_tags(yyjson_val* arr, dc_vec_t* out) {
    if (!arr || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_arr(arr)) return DC_ERROR_INVALID_FORMAT;
    yyjson_arr_iter iter = yyjson_arr_iter_with(arr);
    yyjson_val* val = NULL;
    while ((val = yyjson_arr_iter_next(&iter))) {
        if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
        dc_channel_forum_tag_t tag;
        dc_status_t st = dc_channel_forum_tag_init(&tag);
        if (st != DC_OK) return st;
        dc_snowflake_t id = 0;
        st = dc_json_get_snowflake(val, "id", &id);
        if (st != DC_OK) {
            dc_channel_forum_tag_free(&tag);
            return st;
        }
        tag.id = id;
        const char* name = NULL;
        st = dc_json_get_string(val, "name", &name);
        if (st != DC_OK) {
            dc_channel_forum_tag_free(&tag);
            return st;
        }
        st = dc_json_copy_cstr(&tag.name, name);
        if (st != DC_OK) {
            dc_channel_forum_tag_free(&tag);
            return st;
        }
        int moderated = 0;
        st = dc_json_get_bool_opt(val, "moderated", &moderated, 0);
        if (st != DC_OK) {
            dc_channel_forum_tag_free(&tag);
            return st;
        }
        tag.moderated = moderated;

        dc_optional_snowflake_t emoji_id = {0};
        st = dc_json_get_snowflake_optional_field(val, "emoji_id", &emoji_id);
        if (st != DC_OK) {
            dc_channel_forum_tag_free(&tag);
            return st;
        }
        tag.emoji_id = emoji_id;

        yyjson_val* emoji_name_val = yyjson_obj_get(val, "emoji_name");
        if (emoji_name_val && !yyjson_is_null(emoji_name_val)) {
            if (!yyjson_is_str(emoji_name_val)) {
                dc_channel_forum_tag_free(&tag);
                return DC_ERROR_INVALID_FORMAT;
            }
            tag.emoji_name.is_set = 1;
            st = dc_json_copy_cstr(&tag.emoji_name.value, yyjson_get_str(emoji_name_val));
            if (st != DC_OK) {
                dc_channel_forum_tag_free(&tag);
                return st;
            }
        } else {
            tag.emoji_name.is_set = 0;
        }

        st = dc_vec_push(out, &tag);
        if (st != DC_OK) {
            dc_channel_forum_tag_free(&tag);
            return st;
        }
    }
    return DC_OK;
}

static dc_status_t dc_json_parse_default_reaction(yyjson_val* obj, dc_channel_default_reaction_t* out) {
    if (!obj || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;
    dc_optional_snowflake_t emoji_id = {0};
    dc_status_t st = dc_json_get_snowflake_optional_field(obj, "emoji_id", &emoji_id);
    if (st != DC_OK) return st;
    out->emoji_id = emoji_id;

    yyjson_val* emoji_name_val = yyjson_obj_get(obj, "emoji_name");
    if (emoji_name_val && !yyjson_is_null(emoji_name_val)) {
        if (!yyjson_is_str(emoji_name_val)) return DC_ERROR_INVALID_FORMAT;
        out->emoji_name.is_set = 1;
        st = dc_json_copy_cstr(&out->emoji_name.value, yyjson_get_str(emoji_name_val));
        if (st != DC_OK) return st;
    } else {
        out->emoji_name.is_set = 0;
    }
    return DC_OK;
}

static dc_status_t dc_json_parse_thread_metadata(yyjson_val* obj, dc_channel_thread_metadata_t* out) {
    if (!obj || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;
    int archived = 0;
    dc_status_t st = dc_json_get_bool(obj, "archived", &archived);
    if (st != DC_OK) return st;
    int64_t auto_archive = 0;
    st = dc_json_get_int64(obj, "auto_archive_duration", &auto_archive);
    if (st != DC_OK) return st;
    int auto_archive_int = 0;
    st = dc_int64_to_int_checked(auto_archive, &auto_archive_int);
    if (st != DC_OK) return st;
    const char* archive_ts = NULL;
    st = dc_json_get_string(obj, "archive_timestamp", &archive_ts);
    if (st != DC_OK) return st;
    st = dc_json_parse_iso8601_if_set(archive_ts);
    if (st != DC_OK) return st;
    int locked = 0;
    st = dc_json_get_bool_opt(obj, "locked", &locked, 0);
    if (st != DC_OK) return st;

    out->archived = archived;
    out->auto_archive_duration = auto_archive_int;
    out->locked = locked;

    st = dc_json_copy_cstr(&out->archive_timestamp, archive_ts);
    if (st != DC_OK) return st;

    int invitable = 0;
    st = dc_json_get_bool_opt(obj, "invitable", &invitable, 0);
    if (st != DC_OK) return st;
    out->invitable.is_set = (yyjson_obj_get(obj, "invitable") != NULL);
    out->invitable.value = invitable;

    st = dc_json_get_nullable_string_field(obj, "create_timestamp", &out->create_timestamp, 1);
    if (st != DC_OK) return st;
    if (!out->create_timestamp.is_null) {
        st = dc_json_parse_iso8601_if_set(dc_string_cstr(&out->create_timestamp.value));
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

static dc_status_t dc_json_parse_thread_member(yyjson_val* obj, dc_channel_thread_member_t* out) {
    if (!obj || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;
    dc_status_t st = dc_json_get_snowflake_optional_field(obj, "id", &out->id);
    if (st != DC_OK) return st;
    st = dc_json_get_snowflake_optional_field(obj, "user_id", &out->user_id);
    if (st != DC_OK) return st;
    const char* join_ts = NULL;
    st = dc_json_get_string(obj, "join_timestamp", &join_ts);
    if (st != DC_OK) return st;
    st = dc_json_parse_iso8601_if_set(join_ts);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&out->join_timestamp, join_ts);
    if (st != DC_OK) return st;
    int64_t flags = 0;
    st = dc_json_get_int64(obj, "flags", &flags);
    if (st != DC_OK) return st;
    uint32_t flags_u32 = 0;
    st = dc_int64_to_u32_checked(flags, &flags_u32);
    if (st != DC_OK) return st;
    out->flags = flags_u32;
    return DC_OK;
}

static dc_status_t dc_json_get_optional_tag_bool_field(yyjson_val* obj, const char* key,
                                                       dc_optional_bool_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(obj)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (yyjson_is_null(field)) {
        out->is_set = 1;
        out->value = 1;
        return DC_OK;
    }
    if (!yyjson_is_bool(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_set = 1;
    out->value = yyjson_get_bool(field) ? 1 : 0;
    return DC_OK;
}

static dc_status_t dc_json_mut_add_role_tag_bool_field(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                       const char* key, const dc_optional_bool_t* val) {
    if (!doc || !obj || !key || !val) return DC_ERROR_NULL_POINTER;
    if (!val->is_set) return DC_OK;
    if (val->value) return dc_json_mut_set_null(doc, obj, key);
    return dc_json_mut_set_bool(doc, obj, key, 0);
}

dc_status_t dc_json_model_thread_member_from_val(yyjson_val* val, dc_channel_thread_member_t* member) {
    if (!val || !member) return DC_ERROR_NULL_POINTER;
    return dc_json_parse_thread_member(val, member);
}

dc_status_t dc_json_model_user_from_val(yyjson_val* val, dc_user_t* user) {
    if (!val || !user) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_snowflake_t id = 0;
    dc_status_t st = dc_json_get_snowflake(val, "id", &id);
    if (st != DC_OK) return st;

    const char* username = NULL;
    st = dc_json_get_string(val, "username", &username);
    if (st != DC_OK) return st;

    const char* discriminator = "";
    st = dc_json_get_string_opt(val, "discriminator", &discriminator, "");
    if (st != DC_OK) return st;

    const char* global_name = "";
    st = dc_json_get_string_opt(val, "global_name", &global_name, "");
    if (st != DC_OK) return st;

    const char* avatar = "";
    st = dc_json_get_string_opt(val, "avatar", &avatar, "");
    if (st != DC_OK) return st;

    const char* banner = "";
    st = dc_json_get_string_opt(val, "banner", &banner, "");
    if (st != DC_OK) return st;

    int64_t accent_color_i64 = 0;
    st = dc_json_get_int64_opt(val, "accent_color", &accent_color_i64, 0);
    if (st != DC_OK) return st;
    uint32_t accent_color = 0;
    st = dc_int64_to_u32_checked(accent_color_i64, &accent_color);
    if (st != DC_OK) return st;

    const char* locale = "";
    st = dc_json_get_string_opt(val, "locale", &locale, "");
    if (st != DC_OK) return st;

    const char* email = "";
    st = dc_json_get_string_opt(val, "email", &email, "");
    if (st != DC_OK) return st;

    int64_t flags_i64 = 0;
    st = dc_json_get_int64_opt(val, "flags", &flags_i64, 0);
    if (st != DC_OK) return st;
    uint32_t flags = 0;
    st = dc_int64_to_u32_checked(flags_i64, &flags);
    if (st != DC_OK) return st;

    int64_t premium_i64 = 0;
    st = dc_json_get_int64_opt(val, "premium_type", &premium_i64, 0);
    if (st != DC_OK) return st;
    int premium_int = 0;
    st = dc_int64_to_int_checked(premium_i64, &premium_int);
    if (st != DC_OK) return st;

    int64_t public_flags_i64 = 0;
    st = dc_json_get_int64_opt(val, "public_flags", &public_flags_i64, 0);
    if (st != DC_OK) return st;
    uint32_t public_flags = 0;
    st = dc_int64_to_u32_checked(public_flags_i64, &public_flags);
    if (st != DC_OK) return st;

    const char* avatar_decoration = "";
    st = dc_json_get_string_opt(val, "avatar_decoration", &avatar_decoration, "");
    if (st != DC_OK) return st;

    /* avatar_decoration_data sub-object */
    yyjson_val* add_obj = NULL;
    st = dc_json_get_object_opt(val, "avatar_decoration_data", &add_obj);
    if (st != DC_OK) return st;

    /* collectibles sub-object */
    yyjson_val* collectibles_obj = NULL;
    st = dc_json_get_object_opt(val, "collectibles", &collectibles_obj);
    if (st != DC_OK) return st;

    /* primary_guild sub-object */
    yyjson_val* primary_guild_obj = NULL;
    st = dc_json_get_object_opt(val, "primary_guild", &primary_guild_obj);
    if (st != DC_OK) return st;

    int bot = 0;
    st = dc_json_get_bool_opt(val, "bot", &bot, 0);
    if (st != DC_OK) return st;

    int system = 0;
    st = dc_json_get_bool_opt(val, "system", &system, 0);
    if (st != DC_OK) return st;

    int mfa_enabled = 0;
    st = dc_json_get_bool_opt(val, "mfa_enabled", &mfa_enabled, 0);
    if (st != DC_OK) return st;

    int verified = 0;
    st = dc_json_get_bool_opt(val, "verified", &verified, 0);
    if (st != DC_OK) return st;

    user->id = id;
    user->accent_color = accent_color;
    user->flags = flags;
    user->premium_type = (dc_user_premium_type_t)premium_int;
    user->public_flags = public_flags;
    user->bot = bot;
    user->system = system;
    user->mfa_enabled = mfa_enabled;
    user->verified = verified;

    st = dc_json_copy_cstr(&user->username, username);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&user->discriminator, discriminator);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&user->global_name, global_name);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&user->avatar, avatar);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&user->banner, banner);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&user->locale, locale);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&user->email, email);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&user->avatar_decoration, avatar_decoration);
    if (st != DC_OK) return st;

    /* Parse avatar_decoration_data sub-object */
    if (add_obj) {
        user->has_avatar_decoration_data = 1;
        const char* add_asset = NULL;
        st = dc_json_get_string(add_obj, "asset", &add_asset);
        if (st != DC_OK) return st;
        st = dc_json_copy_cstr(&user->avatar_decoration_data.asset, add_asset);
        if (st != DC_OK) return st;
        st = dc_json_get_snowflake(add_obj, "sku_id", &user->avatar_decoration_data.sku_id);
        if (st != DC_OK) return st;
    }

    /* Parse collectibles sub-object */
    if (collectibles_obj) {
        user->has_collectibles = 1;
        yyjson_val* nameplate_obj = NULL;
        st = dc_json_get_object_opt(collectibles_obj, "nameplate", &nameplate_obj);
        if (st != DC_OK) return st;
        if (nameplate_obj) {
            user->collectibles.has_nameplate = 1;
            st = dc_json_get_snowflake(nameplate_obj, "sku_id",
                                       &user->collectibles.nameplate.sku_id);
            if (st != DC_OK) return st;
            const char* np_asset = NULL;
            st = dc_json_get_string(nameplate_obj, "asset", &np_asset);
            if (st != DC_OK) return st;
            st = dc_json_copy_cstr(&user->collectibles.nameplate.asset, np_asset);
            if (st != DC_OK) return st;
            const char* np_label = NULL;
            st = dc_json_get_string(nameplate_obj, "label", &np_label);
            if (st != DC_OK) return st;
            st = dc_json_copy_cstr(&user->collectibles.nameplate.label, np_label);
            if (st != DC_OK) return st;
            const char* np_palette = NULL;
            st = dc_json_get_string(nameplate_obj, "palette", &np_palette);
            if (st != DC_OK) return st;
            st = dc_json_copy_cstr(&user->collectibles.nameplate.palette, np_palette);
            if (st != DC_OK) return st;
        }
    }

    /* Parse primary_guild sub-object */
    if (primary_guild_obj) {
        user->has_primary_guild = 1;
        st = dc_json_get_nullable_snowflake_field(primary_guild_obj, "identity_guild_id",
                                                  &user->primary_guild.identity_guild_id);
        if (st != DC_OK) return st;
        st = dc_json_get_nullable_bool_field(primary_guild_obj, "identity_enabled",
                                             &user->primary_guild.identity_enabled);
        if (st != DC_OK) return st;
        st = dc_json_get_nullable_string_field(primary_guild_obj, "tag",
                                               &user->primary_guild.tag, 1);
        if (st != DC_OK) return st;
        st = dc_json_get_nullable_string_field(primary_guild_obj, "badge",
                                               &user->primary_guild.badge, 1);
        if (st != DC_OK) return st;
    }

    return DC_OK;
}

dc_status_t dc_json_model_guild_member_from_val(yyjson_val* val, dc_guild_member_t* member) {
    if (!val || !member) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_status_t st = DC_OK;

    yyjson_val* user_val = yyjson_obj_get(val, "user");
    if (user_val && !yyjson_is_null(user_val)) {
        st = dc_json_model_user_from_val(user_val, &member->user);
        if (st != DC_OK) return st;
        member->has_user = 1;
    } else {
        member->has_user = 0;
    }

    st = dc_json_get_nullable_string_field(val, "nick", &member->nick, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "avatar", &member->avatar, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "premium_since", &member->premium_since, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "communication_disabled_until",
                                           &member->communication_disabled_until, 1);
    if (st != DC_OK) return st;

    if (!member->premium_since.is_null) {
        st = dc_json_parse_iso8601_if_set(dc_string_cstr(&member->premium_since.value));
        if (st != DC_OK) return st;
    }
    if (!member->communication_disabled_until.is_null) {
        st = dc_json_parse_iso8601_if_set(dc_string_cstr(&member->communication_disabled_until.value));
        if (st != DC_OK) return st;
    }

    const char* joined_at = "";
    st = dc_json_get_string_opt(val, "joined_at", &joined_at, "");
    if (st != DC_OK) return st;
    st = dc_json_parse_iso8601_if_set(joined_at);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&member->joined_at, joined_at);
    if (st != DC_OK) return st;

    yyjson_val* roles_val = yyjson_obj_get(val, "roles");
    if (roles_val) {
        st = dc_json_parse_snowflake_array(roles_val, &member->roles);
        if (st != DC_OK) return st;
    }

    int deaf = 0;
    st = dc_json_get_bool_opt(val, "deaf", &deaf, 0);
    if (st != DC_OK) return st;
    int mute = 0;
    st = dc_json_get_bool_opt(val, "mute", &mute, 0);
    if (st != DC_OK) return st;
    member->deaf = deaf;
    member->mute = mute;

    yyjson_val* pending_val = yyjson_obj_get(val, "pending");
    if (pending_val && !yyjson_is_null(pending_val)) {
        if (!yyjson_is_bool(pending_val)) return DC_ERROR_INVALID_FORMAT;
        member->pending.is_set = 1;
        member->pending.value = yyjson_get_bool(pending_val) ? 1 : 0;
    } else {
        member->pending.is_set = 0;
        member->pending.value = 0;
    }

    st = dc_json_get_permission_optional_field(val, "permissions", &member->permissions);
    if (st != DC_OK) return st;

    int64_t flags_i64 = 0;
    st = dc_json_get_int64_opt(val, "flags", &flags_i64, (int64_t)0);
    if (st != DC_OK) return st;
    uint32_t flags_u32 = 0;
    st = dc_int64_to_u32_checked(flags_i64, &flags_u32);
    if (st != DC_OK) return st;
    member->flags = flags_u32;

    return DC_OK;
}

dc_status_t dc_json_model_role_from_val(yyjson_val* val, dc_role_t* role) {
    if (!val || !role) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_status_t st = DC_OK;
    dc_snowflake_t id = 0;
    st = dc_json_get_snowflake(val, "id", &id);
    if (st != DC_OK) return st;

    const char* name = NULL;
    st = dc_json_get_string(val, "name", &name);
    if (st != DC_OK) return st;

    int64_t color_i64 = 0;
    st = dc_json_get_int64_opt(val, "color", &color_i64, (int64_t)0);
    if (st != DC_OK) return st;
    uint32_t color = 0;
    st = dc_int64_to_u32_checked(color_i64, &color);
    if (st != DC_OK) return st;

    int hoist = 0;
    st = dc_json_get_bool_opt(val, "hoist", &hoist, 0);
    if (st != DC_OK) return st;

    int64_t position_i64 = 0;
    st = dc_json_get_int64_opt(val, "position", &position_i64, (int64_t)0);
    if (st != DC_OK) return st;
    int position = 0;
    st = dc_int64_to_int_checked(position_i64, &position);
    if (st != DC_OK) return st;

    uint64_t permissions = 0;
    st = dc_json_get_permission_opt(val, "permissions", &permissions, 0);
    if (st != DC_OK) return st;

    int managed = 0;
    st = dc_json_get_bool_opt(val, "managed", &managed, 0);
    if (st != DC_OK) return st;
    int mentionable = 0;
    st = dc_json_get_bool_opt(val, "mentionable", &mentionable, 0);
    if (st != DC_OK) return st;

    int64_t flags_i64 = 0;
    st = dc_json_get_int64_opt(val, "flags", &flags_i64, (int64_t)0);
    if (st != DC_OK) return st;
    uint32_t flags_u32 = 0;
    st = dc_int64_to_u32_checked(flags_i64, &flags_u32);
    if (st != DC_OK) return st;

    role->id = id;
    role->color = color;
    role->hoist = hoist;
    role->position = position;
    role->permissions = permissions;
    role->managed = managed;
    role->mentionable = mentionable;
    role->flags = flags_u32;

    st = dc_json_copy_cstr(&role->name, name);
    if (st != DC_OK) return st;

    st = dc_json_get_nullable_string_field(val, "icon", &role->icon, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "unicode_emoji", &role->unicode_emoji, 1);
    if (st != DC_OK) return st;

    yyjson_val* tags_val = yyjson_obj_get(val, "tags");
    if (tags_val && !yyjson_is_null(tags_val)) {
        if (!yyjson_is_obj(tags_val)) return DC_ERROR_INVALID_FORMAT;

        st = dc_json_get_snowflake_optional_field(tags_val, "bot_id", &role->tags.bot_id);
        if (st != DC_OK) return st;
        st = dc_json_get_snowflake_optional_field(tags_val, "integration_id",
                                                  &role->tags.integration_id);
        if (st != DC_OK) return st;
        st = dc_json_get_snowflake_optional_field(tags_val, "subscription_listing_id",
                                                  &role->tags.subscription_listing_id);
        if (st != DC_OK) return st;

        st = dc_json_get_optional_tag_bool_field(tags_val, "premium_subscriber",
                                                 &role->tags.premium_subscriber);
        if (st != DC_OK) return st;
        st = dc_json_get_optional_tag_bool_field(tags_val, "available_for_purchase",
                                                 &role->tags.available_for_purchase);
        if (st != DC_OK) return st;
        st = dc_json_get_optional_tag_bool_field(tags_val, "guild_connections",
                                                 &role->tags.guild_connections);
        if (st != DC_OK) return st;
    }

    return DC_OK;
}

dc_status_t dc_json_model_channel_from_val(yyjson_val* val, dc_channel_t* channel) {
    if (!val || !channel) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_snowflake_t id = 0;
    dc_status_t st = dc_json_get_snowflake(val, "id", &id);
    if (st != DC_OK) return st;

    int64_t type_i64 = 0;
    st = dc_json_get_int64(val, "type", &type_i64);
    if (st != DC_OK) return st;
    int type_int = 0;
    st = dc_int64_to_int_checked(type_i64, &type_int);
    if (st != DC_OK) return st;

    const char* name = "";
    st = dc_json_get_string_opt(val, "name", &name, "");
    if (st != DC_OK) return st;

    const char* topic = "";
    st = dc_json_get_string_opt(val, "topic", &topic, "");
    if (st != DC_OK) return st;

    const char* icon = "";
    st = dc_json_get_string_opt(val, "icon", &icon, "");
    if (st != DC_OK) return st;

    const char* last_pin = "";
    st = dc_json_get_string_opt(val, "last_pin_timestamp", &last_pin, "");
    if (st != DC_OK) return st;
    st = dc_json_parse_iso8601_if_set(last_pin);
    if (st != DC_OK) return st;

    const char* rtc_region = "";
    st = dc_json_get_string_opt(val, "rtc_region", &rtc_region, "");
    if (st != DC_OK) return st;

    int64_t position_i64 = 0;
    st = dc_json_get_int64_opt(val, "position", &position_i64, 0);
    if (st != DC_OK) return st;
    int position = 0;
    st = dc_int64_to_int_checked(position_i64, &position);
    if (st != DC_OK) return st;

    int nsfw = 0;
    st = dc_json_get_bool_opt(val, "nsfw", &nsfw, 0);
    if (st != DC_OK) return st;

    int64_t bitrate_i64 = 0;
    st = dc_json_get_int64_opt(val, "bitrate", &bitrate_i64, 0);
    if (st != DC_OK) return st;
    int bitrate = 0;
    st = dc_int64_to_int_checked(bitrate_i64, &bitrate);
    if (st != DC_OK) return st;

    int64_t user_limit_i64 = 0;
    st = dc_json_get_int64_opt(val, "user_limit", &user_limit_i64, 0);
    if (st != DC_OK) return st;
    int user_limit = 0;
    st = dc_int64_to_int_checked(user_limit_i64, &user_limit);
    if (st != DC_OK) return st;

    int64_t rate_limit_i64 = 0;
    st = dc_json_get_int64_opt(val, "rate_limit_per_user", &rate_limit_i64, 0);
    if (st != DC_OK) return st;
    int rate_limit = 0;
    st = dc_int64_to_int_checked(rate_limit_i64, &rate_limit);
    if (st != DC_OK) return st;

    int64_t default_auto_archive_i64 = 0;
    st = dc_json_get_int64_opt(val, "default_auto_archive_duration", &default_auto_archive_i64, 0);
    if (st != DC_OK) return st;
    int default_auto_archive = 0;
    st = dc_int64_to_int_checked(default_auto_archive_i64, &default_auto_archive);
    if (st != DC_OK) return st;

    int64_t default_thread_rate_limit_i64 = 0;
    st = dc_json_get_int64_opt(val, "default_thread_rate_limit_per_user",
                               &default_thread_rate_limit_i64, 0);
    if (st != DC_OK) return st;
    int default_thread_rate_limit = 0;
    st = dc_int64_to_int_checked(default_thread_rate_limit_i64, &default_thread_rate_limit);
    if (st != DC_OK) return st;

    int64_t video_quality_i64 = 0;
    st = dc_json_get_int64_opt(val, "video_quality_mode", &video_quality_i64, 0);
    if (st != DC_OK) return st;
    int video_quality = 0;
    st = dc_int64_to_int_checked(video_quality_i64, &video_quality);
    if (st != DC_OK) return st;

    int64_t message_count_i64 = 0;
    st = dc_json_get_int64_opt(val, "message_count", &message_count_i64, 0);
    if (st != DC_OK) return st;
    int message_count = 0;
    st = dc_int64_to_int_checked(message_count_i64, &message_count);
    if (st != DC_OK) return st;

    int64_t member_count_i64 = 0;
    st = dc_json_get_int64_opt(val, "member_count", &member_count_i64, 0);
    if (st != DC_OK) return st;
    int member_count = 0;
    st = dc_int64_to_int_checked(member_count_i64, &member_count);
    if (st != DC_OK) return st;

    int64_t flags_i64 = 0;
    st = dc_json_get_int64_opt(val, "flags", &flags_i64, 0);
    if (st != DC_OK) return st;
    uint32_t flags_u32 = 0;
    st = dc_int64_to_u32_checked(flags_i64, &flags_u32);
    if (st != DC_OK) return st;

    int64_t total_sent_i64 = 0;
    st = dc_json_get_int64_opt(val, "total_message_sent", &total_sent_i64, 0);
    if (st != DC_OK) return st;
    int total_sent = 0;
    st = dc_int64_to_int_checked(total_sent_i64, &total_sent);
    if (st != DC_OK) return st;

    channel->id = id;
    channel->type = (dc_channel_type_t)type_int;
    channel->position = position;
    channel->nsfw = nsfw;
    channel->bitrate = bitrate;
    channel->user_limit = user_limit;
    channel->rate_limit_per_user = rate_limit;
    channel->default_auto_archive_duration = default_auto_archive;
    channel->default_thread_rate_limit_per_user = default_thread_rate_limit;
    channel->video_quality_mode = video_quality;
    channel->message_count = message_count;
    channel->member_count = member_count;
    channel->flags = (uint64_t)flags_u32;
    channel->total_message_sent = total_sent;

    st = dc_json_copy_cstr(&channel->name, name);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&channel->topic, topic);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&channel->icon, icon);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&channel->last_pin_timestamp, last_pin);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&channel->rtc_region, rtc_region);
    if (st != DC_OK) return st;

    st = dc_json_get_snowflake_optional_field(val, "guild_id", &channel->guild_id);
    if (st != DC_OK) return st;
    st = dc_json_get_snowflake_optional_field(val, "parent_id", &channel->parent_id);
    if (st != DC_OK) return st;
    st = dc_json_get_snowflake_optional_field(val, "last_message_id", &channel->last_message_id);
    if (st != DC_OK) return st;
    st = dc_json_get_snowflake_optional_field(val, "owner_id", &channel->owner_id);
    if (st != DC_OK) return st;
    st = dc_json_get_snowflake_optional_field(val, "application_id", &channel->application_id);
    if (st != DC_OK) return st;

    yyjson_val* overwrites_val = yyjson_obj_get(val, "permission_overwrites");
    if (overwrites_val && !yyjson_is_null(overwrites_val)) {
        st = dc_json_parse_permission_overwrites(overwrites_val, &channel->permission_overwrites);
        if (st != DC_OK) return st;
    }

    st = dc_json_get_permission_optional_field(val, "permissions", &channel->permissions);
    if (st != DC_OK) return st;

    yyjson_val* thread_meta_val = yyjson_obj_get(val, "thread_metadata");
    if (thread_meta_val && !yyjson_is_null(thread_meta_val)) {
        st = dc_json_parse_thread_metadata(thread_meta_val, &channel->thread_metadata);
        if (st != DC_OK) return st;
        channel->has_thread_metadata = 1;
    }

    yyjson_val* thread_member_val = yyjson_obj_get(val, "member");
    if (thread_member_val && !yyjson_is_null(thread_member_val)) {
        st = dc_json_parse_thread_member(thread_member_val, &channel->thread_member);
        if (st != DC_OK) return st;
        channel->has_thread_member = 1;
    }

    yyjson_val* available_tags_val = yyjson_obj_get(val, "available_tags");
    if (available_tags_val) {
        st = dc_json_parse_forum_tags(available_tags_val, &channel->available_tags);
        if (st != DC_OK) return st;
    }

    yyjson_val* applied_tags_val = yyjson_obj_get(val, "applied_tags");
    if (applied_tags_val) {
        st = dc_json_parse_snowflake_array(applied_tags_val, &channel->applied_tags);
        if (st != DC_OK) return st;
    }

    yyjson_val* default_reaction_val = yyjson_obj_get(val, "default_reaction_emoji");
    if (default_reaction_val && !yyjson_is_null(default_reaction_val)) {
        st = dc_json_parse_default_reaction(default_reaction_val, &channel->default_reaction_emoji);
        if (st != DC_OK) return st;
        channel->has_default_reaction_emoji = 1;
    }

    int64_t default_sort_i64 = 0;
    st = dc_json_get_int64_opt(val, "default_sort_order", &default_sort_i64, 0);
    if (st != DC_OK) return st;
    int default_sort = 0;
    st = dc_int64_to_int_checked(default_sort_i64, &default_sort);
    if (st != DC_OK) return st;
    channel->default_sort_order = default_sort;

    int64_t default_layout_i64 = 0;
    st = dc_json_get_int64_opt(val, "default_forum_layout", &default_layout_i64, 0);
    if (st != DC_OK) return st;
    int default_layout = 0;
    st = dc_int64_to_int_checked(default_layout_i64, &default_layout);
    if (st != DC_OK) return st;
    channel->default_forum_layout = default_layout;

    return DC_OK;
}

static dc_status_t dc_json_model_message_reference_from_val(yyjson_val* val, dc_message_reference_t* ref) {
    if (!val || !ref) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    int64_t type_i64 = 0;
    dc_status_t st = dc_json_get_int64_opt(val, "type", &type_i64, 0);
    if (st != DC_OK) return st;
    int type_int = 0;
    st = dc_int64_to_int_checked(type_i64, &type_int);
    if (st != DC_OK) return st;
    ref->type = (dc_message_reference_type_t)type_int;

    st = dc_json_get_snowflake_optional_field(val, "message_id", &ref->message_id);
    if (st != DC_OK) return st;
    st = dc_json_get_snowflake_optional_field(val, "channel_id", &ref->channel_id);
    if (st != DC_OK) return st;
    st = dc_json_get_snowflake_optional_field(val, "guild_id", &ref->guild_id);
    if (st != DC_OK) return st;

    st = dc_json_get_bool_opt(val, "fail_if_not_exists", &ref->fail_if_not_exists, 1);
    if (st != DC_OK) return st;

    return DC_OK;
}

static dc_status_t dc_json_model_reaction_from_val(yyjson_val* val, dc_reaction_t* reaction) {
    if (!val || !reaction) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    int64_t count_i64 = 0;
    dc_status_t st = dc_json_get_int64_opt(val, "count", &count_i64, 0);
    if (st != DC_OK) return st;
    int count_int = 0;
    st = dc_int64_to_int_checked(count_i64, &count_int);
    if (st != DC_OK) return st;
    reaction->count = count_int;

    st = dc_json_get_bool_opt(val, "me", &reaction->me, 0);
    if (st != DC_OK) return st;
    st = dc_json_get_bool_opt(val, "me_burst", &reaction->me_burst, 0);
    if (st != DC_OK) return st;

    yyjson_val* count_details_val = yyjson_obj_get(val, "count_details");
    if (count_details_val && yyjson_is_obj(count_details_val)) {
        int64_t burst_i64 = 0, normal_i64 = 0;
        st = dc_json_get_int64_opt(count_details_val, "burst", &burst_i64, 0);
        if (st != DC_OK) return st;
        st = dc_json_get_int64_opt(count_details_val, "normal", &normal_i64, 0);
        if (st != DC_OK) return st;
        int burst_int = 0, normal_int = 0;
        st = dc_int64_to_int_checked(burst_i64, &burst_int);
        if (st != DC_OK) return st;
        st = dc_int64_to_int_checked(normal_i64, &normal_int);
        if (st != DC_OK) return st;
        reaction->count_details.burst = burst_int;
        reaction->count_details.normal = normal_int;
    }

    yyjson_val* emoji_val = yyjson_obj_get(val, "emoji");
    if (emoji_val && yyjson_is_obj(emoji_val)) {
        st = dc_json_get_snowflake_optional_field(emoji_val, "id", &reaction->emoji_id);
        if (st != DC_OK) return st;
        const char* name = "";
        st = dc_json_get_string_opt(emoji_val, "name", &name, "");
        if (st != DC_OK) return st;
        st = dc_json_copy_cstr(&reaction->emoji_name, name);
        if (st != DC_OK) return st;
    }

    yyjson_val* burst_colors_val = yyjson_obj_get(val, "burst_colors");
    if (burst_colors_val && yyjson_is_arr(burst_colors_val)) {
        size_t idx, max;
        yyjson_val* color_val;
        yyjson_arr_foreach(burst_colors_val, idx, max, color_val) {
            if (!yyjson_is_str(color_val)) continue;
            dc_string_t color;
            st = dc_string_init(&color);
            if (st != DC_OK) return st;
            st = dc_json_copy_cstr(&color, yyjson_get_str(color_val));
            if (st != DC_OK) { dc_string_free(&color); return st; }
            st = dc_vec_push(&reaction->burst_colors, &color);
            if (st != DC_OK) { dc_string_free(&color); return st; }
        }
    }

    return DC_OK;
}

static dc_status_t dc_json_model_sticker_item_from_val(yyjson_val* val, dc_sticker_item_t* item) {
    if (!val || !item) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_status_t st = dc_json_get_snowflake(val, "id", &item->id);
    if (st != DC_OK) return st;

    const char* name = "";
    st = dc_json_get_string(val, "name", &name);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&item->name, name);
    if (st != DC_OK) return st;

    int64_t fmt_i64 = 0;
    st = dc_json_get_int64(val, "format_type", &fmt_i64);
    if (st != DC_OK) return st;
    int fmt_int = 0;
    st = dc_int64_to_int_checked(fmt_i64, &fmt_int);
    if (st != DC_OK) return st;
    item->format_type = (dc_sticker_format_type_t)fmt_int;

    return DC_OK;
}

static dc_status_t dc_json_model_channel_mention_from_val(yyjson_val* val, dc_channel_mention_t* mention) {
    if (!val || !mention) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_status_t st = dc_json_get_snowflake(val, "id", &mention->id);
    if (st != DC_OK) return st;
    st = dc_json_get_snowflake(val, "guild_id", &mention->guild_id);
    if (st != DC_OK) return st;

    int64_t type_i64 = 0;
    st = dc_json_get_int64(val, "type", &type_i64);
    if (st != DC_OK) return st;
    int type_int = 0;
    st = dc_int64_to_int_checked(type_i64, &type_int);
    if (st != DC_OK) return st;
    mention->type = type_int;

    const char* name = "";
    st = dc_json_get_string(val, "name", &name);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&mention->name, name);
    if (st != DC_OK) return st;

    return DC_OK;
}

static dc_status_t dc_json_model_role_subscription_data_from_val(yyjson_val* val, dc_role_subscription_data_t* data) {
    if (!val || !data) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_status_t st = dc_json_get_snowflake(val, "role_subscription_listing_id", &data->role_subscription_listing_id);
    if (st != DC_OK) return st;

    const char* tier_name = "";
    st = dc_json_get_string(val, "tier_name", &tier_name);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&data->tier_name, tier_name);
    if (st != DC_OK) return st;

    int64_t months_i64 = 0;
    st = dc_json_get_int64(val, "total_months_subscribed", &months_i64);
    if (st != DC_OK) return st;
    int months_int = 0;
    st = dc_int64_to_int_checked(months_i64, &months_int);
    if (st != DC_OK) return st;
    data->total_months_subscribed = months_int;

    st = dc_json_get_bool(val, "is_renewal", &data->is_renewal);
    if (st != DC_OK) return st;

    return DC_OK;
}

static dc_status_t dc_json_model_message_call_from_val(yyjson_val* val, dc_message_call_t* call) {
    if (!val || !call) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* participants_val = yyjson_obj_get(val, "participants");
    if (participants_val) {
        dc_status_t st = dc_json_parse_snowflake_array(participants_val, &call->participants);
        if (st != DC_OK) return st;
    }

    dc_status_t st = dc_json_get_nullable_string_field(val, "ended_timestamp", &call->ended_timestamp, 1);
    if (st != DC_OK) return st;

    return DC_OK;
}

static dc_status_t dc_json_model_message_activity_from_val(yyjson_val* val, dc_message_activity_t* activity) {
    if (!val || !activity) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    int64_t type_i64 = 0;
    dc_status_t st = dc_json_get_int64(val, "type", &type_i64);
    if (st != DC_OK) return st;
    int type_int = 0;
    st = dc_int64_to_int_checked(type_i64, &type_int);
    if (st != DC_OK) return st;
    activity->type = (dc_message_activity_type_t)type_int;

    yyjson_val* party_id_val = yyjson_obj_get(val, "party_id");
    if (party_id_val && yyjson_is_str(party_id_val)) {
        activity->party_id.is_set = 1;
        st = dc_json_copy_cstr(&activity->party_id.value, yyjson_get_str(party_id_val));
        if (st != DC_OK) return st;
    }

    return DC_OK;
}

static dc_status_t dc_json_get_optional_i32_field(yyjson_val* obj, const char* key, dc_optional_i32_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_int(field) && !yyjson_is_uint(field)) return DC_ERROR_INVALID_FORMAT;
    int64_t v = yyjson_get_sint(field);
    int v_int = 0;
    dc_status_t st = dc_int64_to_int_checked(v, &v_int);
    if (st != DC_OK) return st;
    out->is_set = 1;
    out->value = (int32_t)v_int;
    return DC_OK;
}

static dc_status_t dc_json_get_optional_string_field(yyjson_val* obj, const char* key,
                                                     dc_optional_string_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_set = 0;
        return DC_OK;
    }
    if (yyjson_is_str(field)) {
        out->is_set = 1;
        return dc_json_copy_cstr(&out->value, yyjson_get_str(field));
    }
    /* nonce can be integer or string */
    if (yyjson_is_int(field) || yyjson_is_uint(field)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%" PRId64, yyjson_get_sint(field));
        out->is_set = 1;
        return dc_json_copy_cstr(&out->value, buf);
    }
    return DC_ERROR_INVALID_FORMAT;
}

dc_status_t dc_json_model_message_from_val(yyjson_val* val, dc_message_t* message) {
    if (!val || !message) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_snowflake_t id = 0;
    dc_status_t st = dc_json_get_snowflake(val, "id", &id);
    if (st != DC_OK) return st;

    dc_snowflake_t channel_id = 0;
    st = dc_json_get_snowflake(val, "channel_id", &channel_id);
    if (st != DC_OK) return st;

    yyjson_val* author_val = yyjson_obj_get(val, "author");
    if (!author_val) return DC_ERROR_NOT_FOUND;
    st = dc_json_model_user_from_val(author_val, &message->author);
    if (st != DC_OK) return st;

    const char* content = "";
    st = dc_json_get_string_opt(val, "content", &content, "");
    if (st != DC_OK) return st;

    const char* timestamp = NULL;
    st = dc_json_get_string(val, "timestamp", &timestamp);
    if (st != DC_OK) return st;
    st = dc_json_parse_iso8601_if_set(timestamp);
    if (st != DC_OK) return st;

    st = dc_json_get_nullable_string_field(val, "edited_timestamp", &message->edited_timestamp, 1);
    if (st != DC_OK) return st;
    if (!message->edited_timestamp.is_null) {
        st = dc_json_parse_iso8601_if_set(dc_string_cstr(&message->edited_timestamp.value));
        if (st != DC_OK) return st;
    }

    int tts = 0;
    st = dc_json_get_bool_opt(val, "tts", &tts, 0);
    if (st != DC_OK) return st;
    int mention_everyone = 0;
    st = dc_json_get_bool_opt(val, "mention_everyone", &mention_everyone, 0);
    if (st != DC_OK) return st;
    int pinned = 0;
    st = dc_json_get_bool_opt(val, "pinned", &pinned, 0);
    if (st != DC_OK) return st;

    int64_t type_i64 = 0;
    st = dc_json_get_int64_opt(val, "type", &type_i64, 0);
    if (st != DC_OK) return st;
    int type_int = 0;
    st = dc_int64_to_int_checked(type_i64, &type_int);
    if (st != DC_OK) return st;

    int64_t flags_i64 = 0;
    st = dc_json_get_int64_opt(val, "flags", &flags_i64, 0);
    if (st != DC_OK) return st;
    uint32_t flags_u32 = 0;
    st = dc_int64_to_u32_checked(flags_i64, &flags_u32);
    if (st != DC_OK) return st;

    st = dc_json_copy_cstr(&message->content, content);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&message->timestamp, timestamp);
    if (st != DC_OK) return st;

    message->id = id;
    message->channel_id = channel_id;
    message->tts = tts;
    message->mention_everyone = mention_everyone;
    message->pinned = pinned;
    message->type = (dc_message_type_t)type_int;
    message->flags = (uint64_t)flags_u32;

    st = dc_json_get_snowflake_optional_field(val, "webhook_id", &message->webhook_id);
    if (st != DC_OK) return st;
    st = dc_json_get_snowflake_optional_field(val, "application_id", &message->application_id);
    if (st != DC_OK) return st;

    yyjson_val* mention_roles_val = yyjson_obj_get(val, "mention_roles");
    if (mention_roles_val) {
        st = dc_json_parse_snowflake_array(mention_roles_val, &message->mention_roles);
        if (st != DC_OK) return st;
    }

    yyjson_val* thread_val = yyjson_obj_get(val, "thread");
    if (thread_val && !yyjson_is_null(thread_val)) {
        st = dc_json_model_channel_from_val(thread_val, &message->thread);
        if (st != DC_OK) return st;
        message->has_thread = 1;
    }

    yyjson_val* components_val = yyjson_obj_get(val, "components");
    if (components_val && !yyjson_is_null(components_val)) {
        if (!yyjson_is_arr(components_val)) return DC_ERROR_INVALID_FORMAT;
        yyjson_arr_iter iter = yyjson_arr_iter_with(components_val);
        yyjson_val* component_val = NULL;
        while ((component_val = yyjson_arr_iter_next(&iter))) {
            dc_component_t component;
            st = dc_component_init(&component);
            if (st != DC_OK) return st;
            st = dc_json_model_component_from_val(component_val, &component);
            if (st != DC_OK) {
                dc_component_free(&component);
                return st;
            }
            st = dc_vec_push(&message->components, &component);
            if (st != DC_OK) {
                dc_component_free(&component);
                return st;
            }
        }
    }

    yyjson_val* attachments_val = yyjson_obj_get(val, "attachments");
    if (attachments_val && yyjson_is_arr(attachments_val)) {
        size_t idx, max;
        yyjson_val* att_val;
        yyjson_arr_foreach(attachments_val, idx, max, att_val) {
            dc_attachment_t attachment;
            st = dc_attachment_init(&attachment);
            if (st != DC_OK) return st;
            st = dc_json_model_attachment_from_val(att_val, &attachment);
            if (st != DC_OK) {
                dc_attachment_free(&attachment);
                return st;
            }
            st = dc_vec_push(&message->attachments, &attachment);
            if (st != DC_OK) {
                dc_attachment_free(&attachment);
                return st;
            }
        }
    }

    yyjson_val* embeds_val = yyjson_obj_get(val, "embeds");
    if (embeds_val && yyjson_is_arr(embeds_val)) {
        size_t idx, max;
        yyjson_val* emb_val;
        yyjson_arr_foreach(embeds_val, idx, max, emb_val) {
            dc_embed_t embed;
            st = dc_embed_init(&embed);
            if (st != DC_OK) return st;
            st = dc_json_model_embed_from_val(emb_val, &embed);
            if (st != DC_OK) {
                dc_embed_free(&embed);
                return st;
            }
            st = dc_vec_push(&message->embeds, &embed);
            if (st != DC_OK) {
                dc_embed_free(&embed);
                return st;
            }
        }
    }

    yyjson_val* mentions_val = yyjson_obj_get(val, "mentions");
    if (mentions_val && yyjson_is_arr(mentions_val)) {
        size_t idx, max;
        yyjson_val* men_val;
        yyjson_arr_foreach(mentions_val, idx, max, men_val) {
            dc_guild_member_t mention;
            st = dc_guild_member_init(&mention);
            if (st != DC_OK) return st;
            st = dc_json_model_mention_from_val(men_val, &mention);
             if (st != DC_OK) {
                dc_guild_member_free(&mention);
                return st;
            }
            st = dc_vec_push(&message->mentions, &mention);
            if (st != DC_OK) {
                dc_guild_member_free(&mention);
                return st;
            }
        }
    }
    
    /* message_reference */
    yyjson_val* msg_ref_val = yyjson_obj_get(val, "message_reference");
    if (msg_ref_val && !yyjson_is_null(msg_ref_val)) {
        st = dc_json_model_message_reference_from_val(msg_ref_val, &message->message_reference);
        if (st != DC_OK) return st;
        message->has_message_reference = 1;
    }

    /* referenced_message */
    yyjson_val* ref_msg_val = yyjson_obj_get(val, "referenced_message");
    if (ref_msg_val && !yyjson_is_null(ref_msg_val)) {
        message->referenced_message = (dc_message_t*)dc_calloc((size_t)1, sizeof(dc_message_t));
        if (!message->referenced_message) return DC_ERROR_OUT_OF_MEMORY;
        st = dc_message_init(message->referenced_message);
        if (st != DC_OK) {
            dc_free(message->referenced_message);
            message->referenced_message = NULL;
            return st;
        }
        st = dc_json_model_message_from_val(ref_msg_val, message->referenced_message);
        if (st != DC_OK) {
            dc_message_free(message->referenced_message);
            dc_free(message->referenced_message);
            message->referenced_message = NULL;
            return st;
        }
    }

    /* nonce */
    st = dc_json_get_optional_string_field(val, "nonce", &message->nonce);
    if (st != DC_OK) return st;

    /* reactions */
    yyjson_val* reactions_val = yyjson_obj_get(val, "reactions");
    if (reactions_val && yyjson_is_arr(reactions_val)) {
        size_t idx, max;
        yyjson_val* react_val;
        yyjson_arr_foreach(reactions_val, idx, max, react_val) {
            dc_reaction_t reaction;
            st = dc_reaction_init(&reaction);
            if (st != DC_OK) return st;
            st = dc_json_model_reaction_from_val(react_val, &reaction);
            if (st != DC_OK) {
                dc_reaction_free(&reaction);
                return st;
            }
            st = dc_vec_push(&message->reactions, &reaction);
            if (st != DC_OK) {
                dc_reaction_free(&reaction);
                return st;
            }
        }
    }

    /* sticker_items */
    yyjson_val* sticker_items_val = yyjson_obj_get(val, "sticker_items");
    if (sticker_items_val && yyjson_is_arr(sticker_items_val)) {
        size_t idx, max;
        yyjson_val* si_val;
        yyjson_arr_foreach(sticker_items_val, idx, max, si_val) {
            dc_sticker_item_t item;
            st = dc_sticker_item_init(&item);
            if (st != DC_OK) return st;
            st = dc_json_model_sticker_item_from_val(si_val, &item);
            if (st != DC_OK) {
                dc_sticker_item_free(&item);
                return st;
            }
            st = dc_vec_push(&message->sticker_items, &item);
            if (st != DC_OK) {
                dc_sticker_item_free(&item);
                return st;
            }
        }
    }

    /* mention_channels */
    yyjson_val* mention_channels_val = yyjson_obj_get(val, "mention_channels");
    if (mention_channels_val && yyjson_is_arr(mention_channels_val)) {
        size_t idx, max;
        yyjson_val* mc_val;
        yyjson_arr_foreach(mention_channels_val, idx, max, mc_val) {
            dc_channel_mention_t cm;
            st = dc_channel_mention_init(&cm);
            if (st != DC_OK) return st;
            st = dc_json_model_channel_mention_from_val(mc_val, &cm);
            if (st != DC_OK) {
                dc_channel_mention_free(&cm);
                return st;
            }
            st = dc_vec_push(&message->mention_channels, &cm);
            if (st != DC_OK) {
                dc_channel_mention_free(&cm);
                return st;
            }
        }
    }

    /* position */
    st = dc_json_get_optional_i32_field(val, "position", &message->position);
    if (st != DC_OK) return st;

    /* role_subscription_data */
    yyjson_val* rsd_val = yyjson_obj_get(val, "role_subscription_data");
    if (rsd_val && !yyjson_is_null(rsd_val)) {
        st = dc_json_model_role_subscription_data_from_val(rsd_val, &message->role_subscription_data);
        if (st != DC_OK) return st;
        message->has_role_subscription_data = 1;
    }

    /* call */
    yyjson_val* call_val = yyjson_obj_get(val, "call");
    if (call_val && !yyjson_is_null(call_val)) {
        st = dc_json_model_message_call_from_val(call_val, &message->call);
        if (st != DC_OK) return st;
        message->has_call = 1;
    }

    /* activity */
    yyjson_val* activity_val = yyjson_obj_get(val, "activity");
    if (activity_val && !yyjson_is_null(activity_val)) {
        st = dc_json_model_message_activity_from_val(activity_val, &message->activity);
        if (st != DC_OK) return st;
        message->has_activity = 1;
    }

    return DC_OK;
}

static dc_status_t dc_json_mut_add_optional_snowflake(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                      const char* key, const dc_optional_snowflake_t* opt) {
    if (!doc || !doc->doc || !obj || !key || !opt) return DC_ERROR_NULL_POINTER;
    if (!opt->is_set) return DC_OK;
    return dc_json_mut_set_snowflake(doc, obj, key, opt->value);
}

static dc_status_t dc_json_mut_add_optional_permission(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                       const char* key, const dc_optional_u64_field_t* opt) {
    if (!doc || !doc->doc || !obj || !key || !opt) return DC_ERROR_NULL_POINTER;
    if (!opt->is_set) return DC_OK;
    return dc_json_mut_set_permission(doc, obj, key, opt->value);
}

static dc_status_t dc_json_mut_add_string_if_set(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                 const char* key, const dc_string_t* val) {
    if (!doc || !doc->doc || !obj || !key || !val) return DC_ERROR_NULL_POINTER;
    if (dc_string_is_empty(val)) return DC_OK;
    return dc_json_mut_set_string(doc, obj, key, dc_string_cstr(val));
}

static dc_status_t dc_json_mut_add_nullable_string(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                   const char* key, const dc_nullable_string_t* val) {
    if (!doc || !doc->doc || !obj || !key || !val) return DC_ERROR_NULL_POINTER;
    if (val->is_null) return dc_json_mut_set_null(doc, obj, key);
    if (dc_string_is_empty(&val->value)) return DC_OK;
    return dc_json_mut_set_string(doc, obj, key, dc_string_cstr(&val->value));
}

static dc_status_t dc_json_mut_add_nullable_snowflake(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                      const char* key, const dc_nullable_snowflake_t* val) {
    if (!doc || !doc->doc || !obj || !key || !val) return DC_ERROR_NULL_POINTER;
    if (val->is_null) return dc_json_mut_set_null(doc, obj, key);
    return dc_json_mut_set_snowflake(doc, obj, key, val->value);
}

static dc_status_t dc_json_mut_add_nullable_bool(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                  const char* key, const dc_nullable_bool_t* val) {
    if (!doc || !doc->doc || !obj || !key || !val) return DC_ERROR_NULL_POINTER;
    if (val->is_null) return dc_json_mut_set_null(doc, obj, key);
    return dc_json_mut_set_bool(doc, obj, key, val->value);
}

static dc_status_t dc_json_mut_add_forum_tags(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                              const char* key, const dc_vec_t* tags) {
    if (!doc || !doc->doc || !obj || !key || !tags) return DC_ERROR_NULL_POINTER;
    if (dc_vec_is_empty(tags)) return DC_OK;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;
    yyjson_mut_val* arr = yyjson_mut_obj_add_arr(doc->doc, obj, key);
    if (!arr) return DC_ERROR_OUT_OF_MEMORY;
    for (size_t i = 0; i < dc_vec_length(tags); i++) {
        const dc_channel_forum_tag_t* tag = (const dc_channel_forum_tag_t*)dc_vec_at((dc_vec_t*)tags, i);
        yyjson_mut_val* tag_obj = yyjson_mut_arr_add_obj(doc->doc, arr);
        if (!tag_obj) return DC_ERROR_OUT_OF_MEMORY;
        dc_status_t st = dc_json_mut_set_snowflake(doc, tag_obj, "id", tag->id);
        if (st != DC_OK) return st;
        st = dc_json_mut_set_string(doc, tag_obj, "name", dc_string_cstr(&tag->name));
        if (st != DC_OK) return st;
        st = dc_json_mut_set_bool(doc, tag_obj, "moderated", tag->moderated);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_optional_snowflake(doc, tag_obj, "emoji_id", &tag->emoji_id);
        if (st != DC_OK) return st;
        if (tag->emoji_name.is_set) {
            st = dc_json_mut_set_string(doc, tag_obj, "emoji_name", dc_string_cstr(&tag->emoji_name.value));
            if (st != DC_OK) return st;
        }
    }
    return DC_OK;
}

static dc_status_t dc_json_mut_add_snowflake_array(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                   const char* key, const dc_vec_t* arr_values) {
    if (!doc || !doc->doc || !obj || !key || !arr_values) return DC_ERROR_NULL_POINTER;
    if (dc_vec_is_empty(arr_values)) return DC_OK;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;
    yyjson_mut_val* arr = yyjson_mut_obj_add_arr(doc->doc, obj, key);
    if (!arr) return DC_ERROR_OUT_OF_MEMORY;
    for (size_t i = 0; i < dc_vec_length(arr_values); i++) {
        const dc_snowflake_t* sf = (const dc_snowflake_t*)dc_vec_at((dc_vec_t*)arr_values, i);
        char buf[32];
        dc_status_t st = dc_snowflake_to_cstr(*sf, buf, sizeof(buf));
        if (st != DC_OK) return st;
        if (!yyjson_mut_arr_add_strcpy(doc->doc, arr, buf)) return DC_ERROR_OUT_OF_MEMORY;
    }
    return DC_OK;
}

static dc_status_t dc_json_mut_add_permission_overwrites(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                         const char* key, const dc_vec_t* overwrites) {
    if (!doc || !doc->doc || !obj || !key || !overwrites) return DC_ERROR_NULL_POINTER;
    if (dc_vec_is_empty(overwrites)) return DC_OK;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    yyjson_mut_val* arr = yyjson_mut_obj_add_arr(doc->doc, obj, key);
    if (!arr) return DC_ERROR_OUT_OF_MEMORY;

    for (size_t i = 0; i < dc_vec_length(overwrites); i++) {
        const dc_permission_overwrite_t* ow = (const dc_permission_overwrite_t*)dc_vec_at(overwrites, i);
        yyjson_mut_val* ow_obj = yyjson_mut_arr_add_obj(doc->doc, arr);
        if (!ow_obj) return DC_ERROR_OUT_OF_MEMORY;

        dc_status_t st = dc_json_mut_set_snowflake(doc, ow_obj, "id", ow->id);
        if (st != DC_OK) return st;
        st = dc_json_mut_set_int64(doc, ow_obj, "type", (int64_t)ow->type);
        if (st != DC_OK) return st;
        st = dc_json_mut_set_permission(doc, ow_obj, "allow", ow->allow);
        if (st != DC_OK) return st;
        st = dc_json_mut_set_permission(doc, ow_obj, "deny", ow->deny);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

static dc_status_t dc_json_mut_add_thread_metadata(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                   const char* key, const dc_channel_thread_metadata_t* meta) {
    if (!doc || !doc->doc || !obj || !key || !meta) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;
    yyjson_mut_val* meta_obj = yyjson_mut_obj_add_obj(doc->doc, obj, key);
    if (!meta_obj) return DC_ERROR_OUT_OF_MEMORY;
    dc_status_t st = dc_json_mut_set_bool(doc, meta_obj, "archived", meta->archived);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_int64(doc, meta_obj, "auto_archive_duration", meta->auto_archive_duration);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_string(doc, meta_obj, "archive_timestamp", dc_string_cstr(&meta->archive_timestamp));
    if (st != DC_OK) return st;
    st = dc_json_mut_set_bool(doc, meta_obj, "locked", meta->locked);
    if (st != DC_OK) return st;
    if (meta->invitable.is_set) {
        st = dc_json_mut_set_bool(doc, meta_obj, "invitable", meta->invitable.value);
        if (st != DC_OK) return st;
    }
    st = dc_json_mut_add_nullable_string(doc, meta_obj, "create_timestamp", &meta->create_timestamp);
    return st;
}

static dc_status_t dc_json_mut_add_thread_member(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                 const char* key, const dc_channel_thread_member_t* member) {
    if (!doc || !doc->doc || !obj || !key || !member) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;
    yyjson_mut_val* mem_obj = yyjson_mut_obj_add_obj(doc->doc, obj, key);
    if (!mem_obj) return DC_ERROR_OUT_OF_MEMORY;
    dc_status_t st = dc_json_mut_add_optional_snowflake(doc, mem_obj, "id", &member->id);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_snowflake(doc, mem_obj, "user_id", &member->user_id);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_string(doc, mem_obj, "join_timestamp", dc_string_cstr(&member->join_timestamp));
    if (st != DC_OK) return st;
    st = dc_json_mut_set_int64(doc, mem_obj, "flags", (int64_t)member->flags);
    return st;
}

static dc_status_t dc_json_mut_add_default_reaction(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                    const char* key, const dc_channel_default_reaction_t* emoji) {
    if (!doc || !doc->doc || !obj || !key || !emoji) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;
    yyjson_mut_val* emoji_obj = yyjson_mut_obj_add_obj(doc->doc, obj, key);
    if (!emoji_obj) return DC_ERROR_OUT_OF_MEMORY;
    dc_status_t st = dc_json_mut_add_optional_snowflake(doc, emoji_obj, "emoji_id", &emoji->emoji_id);
    if (st != DC_OK) return st;
    if (emoji->emoji_name.is_set) {
        st = dc_json_mut_set_string(doc, emoji_obj, "emoji_name", dc_string_cstr(&emoji->emoji_name.value));
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

dc_status_t dc_json_model_user_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const dc_user_t* user) {
    if (!doc || !doc->doc || !obj || !user) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;
    dc_status_t st = dc_json_mut_set_snowflake(doc, obj, "id", user->id);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_string(doc, obj, "username", dc_string_cstr(&user->username));
    if (st != DC_OK) return st;
    if (!dc_string_is_empty(&user->discriminator)) {
        st = dc_json_mut_set_string(doc, obj, "discriminator", dc_string_cstr(&user->discriminator));
        if (st != DC_OK) return st;
    }
    st = dc_json_mut_add_string_if_set(doc, obj, "global_name", &user->global_name);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_string_if_set(doc, obj, "avatar", &user->avatar);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_string_if_set(doc, obj, "banner", &user->banner);
    if (st != DC_OK) return st;
    if (user->accent_color != 0) {
        st = dc_json_mut_set_int64(doc, obj, "accent_color", (int64_t)user->accent_color);
        if (st != DC_OK) return st;
    }
    st = dc_json_mut_add_string_if_set(doc, obj, "locale", &user->locale);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_string_if_set(doc, obj, "email", &user->email);
    if (st != DC_OK) return st;
    if (user->flags != 0) {
        st = dc_json_mut_set_int64(doc, obj, "flags", (int64_t)user->flags);
        if (st != DC_OK) return st;
    }
    if (user->premium_type != DC_USER_PREMIUM_NONE) {
        st = dc_json_mut_set_int64(doc, obj, "premium_type", (int64_t)user->premium_type);
        if (st != DC_OK) return st;
    }
    if (user->public_flags != 0) {
        st = dc_json_mut_set_int64(doc, obj, "public_flags", (int64_t)user->public_flags);
        if (st != DC_OK) return st;
    }
    st = dc_json_mut_add_string_if_set(doc, obj, "avatar_decoration", &user->avatar_decoration);
    if (st != DC_OK) return st;

    /* Serialize avatar_decoration_data */
    if (user->has_avatar_decoration_data) {
        yyjson_mut_val* add_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "avatar_decoration_data");
        if (!add_obj) return DC_ERROR_OUT_OF_MEMORY;
        st = dc_json_mut_set_string(doc, add_obj, "asset",
                                    dc_string_cstr(&user->avatar_decoration_data.asset));
        if (st != DC_OK) return st;
        st = dc_json_mut_set_snowflake(doc, add_obj, "sku_id",
                                       user->avatar_decoration_data.sku_id);
        if (st != DC_OK) return st;
    }

    /* Serialize collectibles */
    if (user->has_collectibles) {
        yyjson_mut_val* coll_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "collectibles");
        if (!coll_obj) return DC_ERROR_OUT_OF_MEMORY;
        if (user->collectibles.has_nameplate) {
            yyjson_mut_val* np_obj = yyjson_mut_obj_add_obj(doc->doc, coll_obj, "nameplate");
            if (!np_obj) return DC_ERROR_OUT_OF_MEMORY;
            st = dc_json_mut_set_snowflake(doc, np_obj, "sku_id",
                                           user->collectibles.nameplate.sku_id);
            if (st != DC_OK) return st;
            st = dc_json_mut_set_string(doc, np_obj, "asset",
                                        dc_string_cstr(&user->collectibles.nameplate.asset));
            if (st != DC_OK) return st;
            st = dc_json_mut_set_string(doc, np_obj, "label",
                                        dc_string_cstr(&user->collectibles.nameplate.label));
            if (st != DC_OK) return st;
            st = dc_json_mut_set_string(doc, np_obj, "palette",
                                        dc_string_cstr(&user->collectibles.nameplate.palette));
            if (st != DC_OK) return st;
        }
    }

    /* Serialize primary_guild */
    if (user->has_primary_guild) {
        yyjson_mut_val* pg_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "primary_guild");
        if (!pg_obj) return DC_ERROR_OUT_OF_MEMORY;
        st = dc_json_mut_add_nullable_snowflake(doc, pg_obj, "identity_guild_id",
                                                &user->primary_guild.identity_guild_id);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_nullable_bool(doc, pg_obj, "identity_enabled",
                                           &user->primary_guild.identity_enabled);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_nullable_string(doc, pg_obj, "tag",
                                             &user->primary_guild.tag);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_nullable_string(doc, pg_obj, "badge",
                                             &user->primary_guild.badge);
        if (st != DC_OK) return st;
    }

    if (user->bot) {
        st = dc_json_mut_set_bool(doc, obj, "bot", user->bot);
        if (st != DC_OK) return st;
    }
    if (user->system) {
        st = dc_json_mut_set_bool(doc, obj, "system", user->system);
        if (st != DC_OK) return st;
    }
    if (user->mfa_enabled) {
        st = dc_json_mut_set_bool(doc, obj, "mfa_enabled", user->mfa_enabled);
        if (st != DC_OK) return st;
    }
    if (user->verified) {
        st = dc_json_mut_set_bool(doc, obj, "verified", user->verified);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

dc_status_t dc_json_model_guild_member_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                              const dc_guild_member_t* member) {
    if (!doc || !doc->doc || !obj || !member) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    dc_status_t st = DC_OK;
    if (member->has_user) {
        yyjson_mut_val* user_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "user");
        if (!user_obj) return DC_ERROR_OUT_OF_MEMORY;
        st = dc_json_model_user_to_mut(doc, user_obj, &member->user);
        if (st != DC_OK) return st;
    }

    st = dc_json_mut_add_nullable_string(doc, obj, "nick", &member->nick);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_nullable_string(doc, obj, "avatar", &member->avatar);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_snowflake_array(doc, obj, "roles", &member->roles);
    if (st != DC_OK) return st;

    if (!dc_string_is_empty(&member->joined_at)) {
        st = dc_json_mut_set_string(doc, obj, "joined_at", dc_string_cstr(&member->joined_at));
        if (st != DC_OK) return st;
    }

    st = dc_json_mut_add_nullable_string(doc, obj, "premium_since", &member->premium_since);
    if (st != DC_OK) return st;

    st = dc_json_mut_set_bool(doc, obj, "deaf", member->deaf);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_bool(doc, obj, "mute", member->mute);
    if (st != DC_OK) return st;

    if (member->pending.is_set) {
        st = dc_json_mut_set_bool(doc, obj, "pending", member->pending.value);
        if (st != DC_OK) return st;
    }

    st = dc_json_mut_add_optional_permission(doc, obj, "permissions", &member->permissions);
    if (st != DC_OK) return st;

    st = dc_json_mut_add_nullable_string(doc, obj, "communication_disabled_until",
                                         &member->communication_disabled_until);
    if (st != DC_OK) return st;

    if (member->flags != 0) {
        st = dc_json_mut_set_int64(doc, obj, "flags", (int64_t)member->flags);
        if (st != DC_OK) return st;
    }

    return DC_OK;
}

dc_status_t dc_json_model_role_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const dc_role_t* role) {
    if (!doc || !doc->doc || !obj || !role) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    dc_status_t st = dc_json_mut_set_snowflake(doc, obj, "id", role->id);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_string(doc, obj, "name", dc_string_cstr(&role->name));
    if (st != DC_OK) return st;
    st = dc_json_mut_set_int64(doc, obj, "color", (int64_t)role->color);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_bool(doc, obj, "hoist", role->hoist);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_nullable_string(doc, obj, "icon", &role->icon);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_nullable_string(doc, obj, "unicode_emoji", &role->unicode_emoji);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_int64(doc, obj, "position", role->position);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_permission(doc, obj, "permissions", role->permissions);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_bool(doc, obj, "managed", role->managed);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_bool(doc, obj, "mentionable", role->mentionable);
    if (st != DC_OK) return st;
    if (role->flags != 0) {
        st = dc_json_mut_set_int64(doc, obj, "flags", (int64_t)role->flags);
        if (st != DC_OK) return st;
    }

    if (role->tags.bot_id.is_set || role->tags.integration_id.is_set ||
        role->tags.subscription_listing_id.is_set || role->tags.premium_subscriber.is_set ||
        role->tags.available_for_purchase.is_set || role->tags.guild_connections.is_set) {
        yyjson_mut_val* tags_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "tags");
        if (!tags_obj) return DC_ERROR_OUT_OF_MEMORY;

        st = dc_json_mut_add_optional_snowflake(doc, tags_obj, "bot_id", &role->tags.bot_id);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_optional_snowflake(doc, tags_obj, "integration_id",
                                                &role->tags.integration_id);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_optional_snowflake(doc, tags_obj, "subscription_listing_id",
                                                &role->tags.subscription_listing_id);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_role_tag_bool_field(doc, tags_obj, "premium_subscriber",
                                                 &role->tags.premium_subscriber);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_role_tag_bool_field(doc, tags_obj, "available_for_purchase",
                                                 &role->tags.available_for_purchase);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_role_tag_bool_field(doc, tags_obj, "guild_connections",
                                                 &role->tags.guild_connections);
        if (st != DC_OK) return st;
    }

    return DC_OK;
}

dc_status_t dc_json_model_channel_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const dc_channel_t* channel) {
    if (!doc || !doc->doc || !obj || !channel) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    dc_status_t st = dc_json_mut_set_snowflake(doc, obj, "id", channel->id);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_int64(doc, obj, "type", (int64_t)channel->type);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_snowflake(doc, obj, "guild_id", &channel->guild_id);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_snowflake(doc, obj, "parent_id", &channel->parent_id);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_snowflake(doc, obj, "last_message_id", &channel->last_message_id);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_snowflake(doc, obj, "owner_id", &channel->owner_id);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_snowflake(doc, obj, "application_id", &channel->application_id);
    if (st != DC_OK) return st;

    st = dc_json_mut_add_permission_overwrites(doc, obj, "permission_overwrites", &channel->permission_overwrites);
    if (st != DC_OK) return st;

    st = dc_json_mut_add_string_if_set(doc, obj, "name", &channel->name);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_string_if_set(doc, obj, "topic", &channel->topic);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_string_if_set(doc, obj, "icon", &channel->icon);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_string_if_set(doc, obj, "last_pin_timestamp", &channel->last_pin_timestamp);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_string_if_set(doc, obj, "rtc_region", &channel->rtc_region);
    if (st != DC_OK) return st;

    if (channel->position != 0) {
        st = dc_json_mut_set_int64(doc, obj, "position", channel->position);
        if (st != DC_OK) return st;
    }
    if (channel->nsfw) {
        st = dc_json_mut_set_bool(doc, obj, "nsfw", channel->nsfw);
        if (st != DC_OK) return st;
    }
    if (channel->bitrate != 0) {
        st = dc_json_mut_set_int64(doc, obj, "bitrate", channel->bitrate);
        if (st != DC_OK) return st;
    }
    if (channel->user_limit != 0) {
        st = dc_json_mut_set_int64(doc, obj, "user_limit", channel->user_limit);
        if (st != DC_OK) return st;
    }
    if (channel->rate_limit_per_user != 0) {
        st = dc_json_mut_set_int64(doc, obj, "rate_limit_per_user", channel->rate_limit_per_user);
        if (st != DC_OK) return st;
    }
    if (channel->default_auto_archive_duration != 0) {
        st = dc_json_mut_set_int64(doc, obj, "default_auto_archive_duration", channel->default_auto_archive_duration);
        if (st != DC_OK) return st;
    }
    if (channel->default_thread_rate_limit_per_user != 0) {
        st = dc_json_mut_set_int64(doc, obj, "default_thread_rate_limit_per_user",
                                   channel->default_thread_rate_limit_per_user);
        if (st != DC_OK) return st;
    }
    if (channel->video_quality_mode != 0) {
        st = dc_json_mut_set_int64(doc, obj, "video_quality_mode", channel->video_quality_mode);
        if (st != DC_OK) return st;
    }
    if (channel->message_count != 0) {
        st = dc_json_mut_set_int64(doc, obj, "message_count", channel->message_count);
        if (st != DC_OK) return st;
    }
    if (channel->member_count != 0) {
        st = dc_json_mut_set_int64(doc, obj, "member_count", channel->member_count);
        if (st != DC_OK) return st;
    }
    if (channel->flags != 0) {
        st = dc_json_mut_set_int64(doc, obj, "flags", (int64_t)channel->flags);
        if (st != DC_OK) return st;
    }
    if (channel->total_message_sent != 0) {
        st = dc_json_mut_set_int64(doc, obj, "total_message_sent", channel->total_message_sent);
        if (st != DC_OK) return st;
    }

    st = dc_json_mut_add_optional_permission(doc, obj, "permissions", &channel->permissions);
    if (st != DC_OK) return st;

    if (channel->has_thread_metadata) {
        st = dc_json_mut_add_thread_metadata(doc, obj, "thread_metadata", &channel->thread_metadata);
        if (st != DC_OK) return st;
    }
    if (channel->has_thread_member) {
        st = dc_json_mut_add_thread_member(doc, obj, "member", &channel->thread_member);
        if (st != DC_OK) return st;
    }
    st = dc_json_mut_add_forum_tags(doc, obj, "available_tags", &channel->available_tags);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_snowflake_array(doc, obj, "applied_tags", &channel->applied_tags);
    if (st != DC_OK) return st;
    if (channel->has_default_reaction_emoji) {
        st = dc_json_mut_add_default_reaction(doc, obj, "default_reaction_emoji", &channel->default_reaction_emoji);
        if (st != DC_OK) return st;
    }
    if (channel->default_sort_order != 0) {
        st = dc_json_mut_set_int64(doc, obj, "default_sort_order", channel->default_sort_order);
        if (st != DC_OK) return st;
    }
    if (channel->default_forum_layout != 0) {
        st = dc_json_mut_set_int64(doc, obj, "default_forum_layout", channel->default_forum_layout);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

dc_status_t dc_json_model_message_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const dc_message_t* message) {
    if (!doc || !doc->doc || !obj || !message) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    dc_status_t st = dc_json_mut_set_snowflake(doc, obj, "id", message->id);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_snowflake(doc, obj, "channel_id", message->channel_id);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_string(doc, obj, "content", dc_string_cstr(&message->content));
    if (st != DC_OK) return st;
    st = dc_json_mut_set_string(doc, obj, "timestamp", dc_string_cstr(&message->timestamp));
    if (st != DC_OK) return st;
    st = dc_json_mut_add_nullable_string(doc, obj, "edited_timestamp", &message->edited_timestamp);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_bool(doc, obj, "tts", message->tts);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_bool(doc, obj, "mention_everyone", message->mention_everyone);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_bool(doc, obj, "pinned", message->pinned);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_int64(doc, obj, "type", (int64_t)message->type);
    if (st != DC_OK) return st;
    if (message->flags != 0) {
        st = dc_json_mut_set_int64(doc, obj, "flags", (int64_t)message->flags);
        if (st != DC_OK) return st;
    }

    st = dc_json_mut_add_optional_snowflake(doc, obj, "webhook_id", &message->webhook_id);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_snowflake(doc, obj, "application_id", &message->application_id);
    if (st != DC_OK) return st;

    st = dc_json_mut_add_snowflake_array(doc, obj, "mention_roles", &message->mention_roles);
    if (st != DC_OK) return st;

    if (message->has_thread) {
        yyjson_mut_val* thread_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "thread");
        if (!thread_obj) return DC_ERROR_OUT_OF_MEMORY;
        st = dc_json_model_channel_to_mut(doc, thread_obj, &message->thread);
        if (st != DC_OK) return st;
    }

    if (!dc_vec_is_empty(&message->components)) {
        yyjson_mut_val* components_arr = yyjson_mut_obj_add_arr(doc->doc, obj, "components");
        if (!components_arr) return DC_ERROR_OUT_OF_MEMORY;
        for (size_t i = 0; i < dc_vec_length(&message->components); i++) {
            const dc_component_t* component = dc_vec_at(&message->components, i);
            yyjson_mut_val* component_obj = yyjson_mut_arr_add_obj(doc->doc, components_arr);
            if (!component_obj) return DC_ERROR_OUT_OF_MEMORY;
            st = dc_json_model_component_to_mut(doc, component_obj, component);
            if (st != DC_OK) return st;
        }
    }

    yyjson_mut_val* author_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "author");
    if (!author_obj) return DC_ERROR_OUT_OF_MEMORY;
    st = dc_json_model_user_to_mut(doc, author_obj, &message->author);
    if (st != DC_OK) return st;

    /* message_reference */
    if (message->has_message_reference) {
        yyjson_mut_val* ref_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "message_reference");
        if (!ref_obj) return DC_ERROR_OUT_OF_MEMORY;
        st = dc_json_mut_set_int64(doc, ref_obj, "type", (int64_t)message->message_reference.type);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_optional_snowflake(doc, ref_obj, "message_id", &message->message_reference.message_id);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_optional_snowflake(doc, ref_obj, "channel_id", &message->message_reference.channel_id);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_optional_snowflake(doc, ref_obj, "guild_id", &message->message_reference.guild_id);
        if (st != DC_OK) return st;
    }

    /* nonce */
    if (message->nonce.is_set) {
        st = dc_json_mut_set_string(doc, obj, "nonce", dc_string_cstr(&message->nonce.value));
        if (st != DC_OK) return st;
    }

    /* reactions */
    if (!dc_vec_is_empty(&message->reactions)) {
        yyjson_mut_val* reactions_arr = yyjson_mut_obj_add_arr(doc->doc, obj, "reactions");
        if (!reactions_arr) return DC_ERROR_OUT_OF_MEMORY;
        for (size_t i = 0; i < dc_vec_length(&message->reactions); i++) {
            const dc_reaction_t* reaction = dc_vec_at(&message->reactions, i);
            yyjson_mut_val* react_obj = yyjson_mut_arr_add_obj(doc->doc, reactions_arr);
            if (!react_obj) return DC_ERROR_OUT_OF_MEMORY;
            st = dc_json_mut_set_int64(doc, react_obj, "count", (int64_t)reaction->count);
            if (st != DC_OK) return st;
            st = dc_json_mut_set_bool(doc, react_obj, "me", reaction->me);
            if (st != DC_OK) return st;
            st = dc_json_mut_set_bool(doc, react_obj, "me_burst", reaction->me_burst);
            if (st != DC_OK) return st;

            yyjson_mut_val* cd_obj = yyjson_mut_obj_add_obj(doc->doc, react_obj, "count_details");
            if (!cd_obj) return DC_ERROR_OUT_OF_MEMORY;
            st = dc_json_mut_set_int64(doc, cd_obj, "burst", (int64_t)reaction->count_details.burst);
            if (st != DC_OK) return st;
            st = dc_json_mut_set_int64(doc, cd_obj, "normal", (int64_t)reaction->count_details.normal);
            if (st != DC_OK) return st;

            yyjson_mut_val* emoji_obj = yyjson_mut_obj_add_obj(doc->doc, react_obj, "emoji");
            if (!emoji_obj) return DC_ERROR_OUT_OF_MEMORY;
            st = dc_json_mut_add_optional_snowflake(doc, emoji_obj, "id", &reaction->emoji_id);
            if (st != DC_OK) return st;
            st = dc_json_mut_add_string_if_set(doc, emoji_obj, "name", &reaction->emoji_name);
            if (st != DC_OK) return st;

            if (!dc_vec_is_empty(&reaction->burst_colors)) {
                yyjson_mut_val* bc_arr = yyjson_mut_obj_add_arr(doc->doc, react_obj, "burst_colors");
                if (!bc_arr) return DC_ERROR_OUT_OF_MEMORY;
                for (size_t j = 0; j < dc_vec_length(&reaction->burst_colors); j++) {
                    const dc_string_t* color = dc_vec_at(&reaction->burst_colors, j);
                    if (!yyjson_mut_arr_add_strcpy(doc->doc, bc_arr, dc_string_cstr(color)))
                        return DC_ERROR_OUT_OF_MEMORY;
                }
            }
        }
    }

    /* sticker_items */
    if (!dc_vec_is_empty(&message->sticker_items)) {
        yyjson_mut_val* si_arr = yyjson_mut_obj_add_arr(doc->doc, obj, "sticker_items");
        if (!si_arr) return DC_ERROR_OUT_OF_MEMORY;
        for (size_t i = 0; i < dc_vec_length(&message->sticker_items); i++) {
            const dc_sticker_item_t* item = dc_vec_at(&message->sticker_items, i);
            yyjson_mut_val* si_obj = yyjson_mut_arr_add_obj(doc->doc, si_arr);
            if (!si_obj) return DC_ERROR_OUT_OF_MEMORY;
            st = dc_json_mut_set_snowflake(doc, si_obj, "id", item->id);
            if (st != DC_OK) return st;
            st = dc_json_mut_set_string(doc, si_obj, "name", dc_string_cstr(&item->name));
            if (st != DC_OK) return st;
            st = dc_json_mut_set_int64(doc, si_obj, "format_type", (int64_t)item->format_type);
            if (st != DC_OK) return st;
        }
    }

    /* mention_channels */
    if (!dc_vec_is_empty(&message->mention_channels)) {
        yyjson_mut_val* mc_arr = yyjson_mut_obj_add_arr(doc->doc, obj, "mention_channels");
        if (!mc_arr) return DC_ERROR_OUT_OF_MEMORY;
        for (size_t i = 0; i < dc_vec_length(&message->mention_channels); i++) {
            const dc_channel_mention_t* cm = dc_vec_at(&message->mention_channels, i);
            yyjson_mut_val* mc_obj = yyjson_mut_arr_add_obj(doc->doc, mc_arr);
            if (!mc_obj) return DC_ERROR_OUT_OF_MEMORY;
            st = dc_json_mut_set_snowflake(doc, mc_obj, "id", cm->id);
            if (st != DC_OK) return st;
            st = dc_json_mut_set_snowflake(doc, mc_obj, "guild_id", cm->guild_id);
            if (st != DC_OK) return st;
            st = dc_json_mut_set_int64(doc, mc_obj, "type", (int64_t)cm->type);
            if (st != DC_OK) return st;
            st = dc_json_mut_set_string(doc, mc_obj, "name", dc_string_cstr(&cm->name));
            if (st != DC_OK) return st;
        }
    }

    /* position */
    if (message->position.is_set) {
        st = dc_json_mut_set_int64(doc, obj, "position", (int64_t)message->position.value);
        if (st != DC_OK) return st;
    }

    /* role_subscription_data */
    if (message->has_role_subscription_data) {
        yyjson_mut_val* rsd_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "role_subscription_data");
        if (!rsd_obj) return DC_ERROR_OUT_OF_MEMORY;
        st = dc_json_mut_set_snowflake(doc, rsd_obj, "role_subscription_listing_id",
                                       message->role_subscription_data.role_subscription_listing_id);
        if (st != DC_OK) return st;
        st = dc_json_mut_set_string(doc, rsd_obj, "tier_name",
                                    dc_string_cstr(&message->role_subscription_data.tier_name));
        if (st != DC_OK) return st;
        st = dc_json_mut_set_int64(doc, rsd_obj, "total_months_subscribed",
                                   (int64_t)message->role_subscription_data.total_months_subscribed);
        if (st != DC_OK) return st;
        st = dc_json_mut_set_bool(doc, rsd_obj, "is_renewal", message->role_subscription_data.is_renewal);
        if (st != DC_OK) return st;
    }

    /* call */
    if (message->has_call) {
        yyjson_mut_val* call_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "call");
        if (!call_obj) return DC_ERROR_OUT_OF_MEMORY;
        st = dc_json_mut_add_snowflake_array(doc, call_obj, "participants", &message->call.participants);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_nullable_string(doc, call_obj, "ended_timestamp", &message->call.ended_timestamp);
        if (st != DC_OK) return st;
    }

    /* activity */
    if (message->has_activity) {
        yyjson_mut_val* act_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "activity");
        if (!act_obj) return DC_ERROR_OUT_OF_MEMORY;
        st = dc_json_mut_set_int64(doc, act_obj, "type", (int64_t)message->activity.type);
        if (st != DC_OK) return st;
        if (message->activity.party_id.is_set) {
            st = dc_json_mut_set_string(doc, act_obj, "party_id",
                                        dc_string_cstr(&message->activity.party_id.value));
            if (st != DC_OK) return st;
        }
    }

    return DC_OK;
}

dc_status_t dc_json_model_voice_state_from_val(yyjson_val* val, dc_voice_state_t* vs) {
    if (!val || !vs) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_status_t st = dc_json_get_snowflake_optional_field(val, "guild_id", &vs->guild_id);
    if (st != DC_OK) return st;

    st = dc_json_get_snowflake(val, "channel_id", &vs->channel_id);
    if (st != DC_OK) return st;

    st = dc_json_get_snowflake(val, "user_id", &vs->user_id);
    if (st != DC_OK) return st;

    const char* session_id = "";
    st = dc_json_get_string_opt(val, "session_id", &session_id, "");
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&vs->session_id, session_id);
    if (st != DC_OK) return st;

    st = dc_json_get_bool_opt(val, "deaf", &vs->deaf, 0);
    if (st != DC_OK) return st;
    st = dc_json_get_bool_opt(val, "mute", &vs->mute, 0);
    if (st != DC_OK) return st;
    st = dc_json_get_bool_opt(val, "self_deaf", &vs->self_deaf, 0);
    if (st != DC_OK) return st;
    st = dc_json_get_bool_opt(val, "self_mute", &vs->self_mute, 0);
    if (st != DC_OK) return st;
    st = dc_json_get_bool_opt(val, "self_stream", &vs->self_stream, 0);
    if (st != DC_OK) return st;
    st = dc_json_get_bool_opt(val, "self_video", &vs->self_video, 0);
    if (st != DC_OK) return st;
    st = dc_json_get_bool_opt(val, "suppress", &vs->suppress, 0);
    if (st != DC_OK) return st;

    st = dc_json_get_nullable_string_field(val, "request_to_speak_timestamp",
                                            &vs->request_to_speak_timestamp, 1);
    if (st != DC_OK) return st;

    return DC_OK;
}

dc_status_t dc_json_model_presence_from_val(yyjson_val* val, dc_presence_t* presence) {
    if (!val || !presence) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    /* The user field is a partial user object with only id */
    yyjson_val* user_val = yyjson_obj_get(val, "user");
    if (user_val && yyjson_is_obj(user_val)) {
        dc_status_t st = dc_json_get_snowflake(user_val, "id", &presence->user_id);
        if (st != DC_OK) return st;
    }

    const char* status = "offline";
    dc_status_t st = dc_json_get_string_opt(val, "status", &status, "offline");
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&presence->status_str, status);
    if (st != DC_OK) return st;
    presence->status = dc_presence_status_from_string(status);

    return DC_OK;
}

dc_status_t dc_json_model_attachment_from_val(yyjson_val* val, dc_attachment_t* attachment) {
    if (!val || !attachment) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_status_t st = dc_json_get_snowflake(val, "id", &attachment->id);
    if (st != DC_OK) return st;

    const char* filename = "";
    st = dc_json_get_string(val, "filename", &filename);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&attachment->filename, filename);
    if (st != DC_OK) return st;

    st = dc_json_get_nullable_string_field(val, "description", &attachment->description, 1);
    if (st != DC_OK) return st;

    st = dc_json_get_nullable_string_field(val, "content_type", &attachment->content_type, 1);
    if (st != DC_OK) return st;

    int64_t size = 0;
    st = dc_json_get_int64(val, "size", &size);
    if (st != DC_OK) return st;
    attachment->size = (size_t)size;

    const char* url = "";
    st = dc_json_get_string(val, "url", &url);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&attachment->url, url);
    if (st != DC_OK) return st;

    const char* proxy_url = "";
    st = dc_json_get_string(val, "proxy_url", &proxy_url);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&attachment->proxy_url, proxy_url);
    if (st != DC_OK) return st;

    /* Optional height/width/ephemeral/flags */
    yyjson_val* height_val = yyjson_obj_get(val, "height");
    if (!height_val || yyjson_is_null(height_val)) {
        attachment->height.is_set = 0;
        attachment->height.value = 0;
    } else if (yyjson_is_int(height_val)) {
        int64_t h = yyjson_get_sint(height_val);
        if (h < 0) return DC_ERROR_INVALID_FORMAT;
        attachment->height.is_set = 1;
        attachment->height.value = (dc_snowflake_t)h;
    } else if (yyjson_is_uint(height_val)) {
        attachment->height.is_set = 1;
        attachment->height.value = (dc_snowflake_t)yyjson_get_uint(height_val);
    } else {
        return DC_ERROR_INVALID_FORMAT;
    }

    yyjson_val* width_val = yyjson_obj_get(val, "width");
    if (!width_val || yyjson_is_null(width_val)) {
        attachment->width.is_set = 0;
        attachment->width.value = 0;
    } else if (yyjson_is_int(width_val)) {
        int64_t w = yyjson_get_sint(width_val);
        if (w < 0) return DC_ERROR_INVALID_FORMAT;
        attachment->width.is_set = 1;
        attachment->width.value = (dc_snowflake_t)w;
    } else if (yyjson_is_uint(width_val)) {
        attachment->width.is_set = 1;
        attachment->width.value = (dc_snowflake_t)yyjson_get_uint(width_val);
    } else {
        return DC_ERROR_INVALID_FORMAT;
    }

    st = dc_json_get_bool_opt(val, "ephemeral", &attachment->ephemeral, 0);
    if (st != DC_OK) return st;

    return DC_OK;
}

static dc_status_t dc_json_model_embed_footer_from_val(yyjson_val* val, dc_embed_footer_t* footer) {
    if (!val || !footer) return DC_ERROR_NULL_POINTER;
    const char* text = "";
    dc_status_t st = dc_json_get_string(val, "text", &text);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&footer->text, text);
    if (st != DC_OK) return st;

    st = dc_json_get_nullable_string_field(val, "icon_url", &footer->icon_url, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "proxy_icon_url", &footer->proxy_icon_url, 1);
    if (st != DC_OK) return st;
    return DC_OK;
}

static dc_status_t dc_json_model_embed_image_from_val(yyjson_val* val, dc_embed_image_t* img) {
    if (!val || !img) return DC_ERROR_NULL_POINTER;
    const char* url = "";
    dc_status_t st = dc_json_get_string(val, "url", &url);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&img->url, url);
    if (st != DC_OK) return st;

    st = dc_json_get_nullable_string_field(val, "proxy_url", &img->proxy_url, 1);
    if (st != DC_OK) return st;
    
    int64_t h=0, w=0;
    dc_json_get_int64_opt(val, "height", &h, 0);
    dc_json_get_int64_opt(val, "width", &w, 0);
    img->height = (int)h;
    img->width = (int)w;
    return DC_OK;
}

static dc_status_t dc_json_model_embed_thumbnail_from_val(yyjson_val* val, dc_embed_thumbnail_t* thumb) {
    if (!val || !thumb) return DC_ERROR_NULL_POINTER;
    const char* url = "";
    dc_status_t st = dc_json_get_string(val, "url", &url);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&thumb->url, url);
    if (st != DC_OK) return st;

    st = dc_json_get_nullable_string_field(val, "proxy_url", &thumb->proxy_url, 1);
    if (st != DC_OK) return st;

    int64_t h=0, w=0;
    dc_json_get_int64_opt(val, "height", &h, 0);
    dc_json_get_int64_opt(val, "width", &w, 0);
    thumb->height = (int)h;
    thumb->width = (int)w;
    return DC_OK;
}

static dc_status_t dc_json_model_embed_video_from_val(yyjson_val* val, dc_embed_video_t* video) {
    if (!val || !video) return DC_ERROR_NULL_POINTER;
    dc_status_t st = dc_json_get_nullable_string_field(val, "url", &video->url, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "proxy_url", &video->proxy_url, 1);
    if (st != DC_OK) return st;
    
    int64_t h=0, w=0;
    dc_json_get_int64_opt(val, "height", &h, 0);
    dc_json_get_int64_opt(val, "width", &w, 0);
    video->height = (int)h;
    video->width = (int)w;
    return DC_OK;
}

static dc_status_t dc_json_model_embed_provider_from_val(yyjson_val* val, dc_embed_provider_t* prov) {
    if (!val || !prov) return DC_ERROR_NULL_POINTER;
    dc_status_t st = dc_json_get_nullable_string_field(val, "name", &prov->name, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "url", &prov->url, 1);
    if (st != DC_OK) return st;
    return DC_OK;
}

static dc_status_t dc_json_model_embed_author_from_val(yyjson_val* val, dc_embed_author_t* auth) {
    if (!val || !auth) return DC_ERROR_NULL_POINTER;
    const char* name = "";
    dc_status_t st = dc_json_get_string(val, "name", &name);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&auth->name, name);
    if (st != DC_OK) return st;

    st = dc_json_get_nullable_string_field(val, "url", &auth->url, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "icon_url", &auth->icon_url, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "proxy_icon_url", &auth->proxy_icon_url, 1);
    if (st != DC_OK) return st;
    return DC_OK;
}

static dc_status_t dc_json_model_embed_field_from_val(yyjson_val* val, dc_embed_field_t* field) {
    if (!val || !field) return DC_ERROR_NULL_POINTER;
    const char* name = "";
    dc_status_t st = dc_json_get_string(val, "name", &name);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&field->name, name);
    if (st != DC_OK) return st;

    const char* value = "";
    st = dc_json_get_string(val, "value", &value);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&field->value, value);
    if (st != DC_OK) return st;
    
    st = dc_json_get_bool_opt(val, "inline", &field->_inline, 0);
    if (st != DC_OK) return st;
    return DC_OK;
}

dc_status_t dc_json_model_embed_from_val(yyjson_val* val, dc_embed_t* embed) {
    if (!val || !embed) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_status_t st = dc_json_get_nullable_string_field(val, "title", &embed->title, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "type", &embed->type, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "description", &embed->description, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "url", &embed->url, 1);
    if (st != DC_OK) return st;
    st = dc_json_get_nullable_string_field(val, "timestamp", &embed->timestamp, 1);
    if (st != DC_OK) return st;
    
    int64_t color = 0;
    st = dc_json_get_int64_opt(val, "color", &color, 0);
    if (st != DC_OK) return st;
    embed->color = (int)color;

    yyjson_val* footer = yyjson_obj_get(val, "footer");
    if (footer && !yyjson_is_null(footer)) {
        if (!yyjson_is_obj(footer)) return DC_ERROR_INVALID_FORMAT;
        embed->has_footer = 1;
        st = dc_json_model_embed_footer_from_val(footer, &embed->footer);
        if (st != DC_OK) return st;
    }

    yyjson_val* image = yyjson_obj_get(val, "image");
    if (image && !yyjson_is_null(image)) {
        if (!yyjson_is_obj(image)) return DC_ERROR_INVALID_FORMAT;
        embed->has_image = 1;
        st = dc_json_model_embed_image_from_val(image, &embed->image);
        if (st != DC_OK) return st;
    }

    yyjson_val* thumbnail = yyjson_obj_get(val, "thumbnail");
    if (thumbnail && !yyjson_is_null(thumbnail)) {
        if (!yyjson_is_obj(thumbnail)) return DC_ERROR_INVALID_FORMAT;
        embed->has_thumbnail = 1;
        st = dc_json_model_embed_thumbnail_from_val(thumbnail, &embed->thumbnail);
        if (st != DC_OK) return st;
    }

    yyjson_val* video = yyjson_obj_get(val, "video");
    if (video && !yyjson_is_null(video)) {
        if (!yyjson_is_obj(video)) return DC_ERROR_INVALID_FORMAT;
        embed->has_video = 1;
        st = dc_json_model_embed_video_from_val(video, &embed->video);
        if (st != DC_OK) return st;
    }

    yyjson_val* provider = yyjson_obj_get(val, "provider");
    if (provider && !yyjson_is_null(provider)) {
        if (!yyjson_is_obj(provider)) return DC_ERROR_INVALID_FORMAT;
        embed->has_provider = 1;
        st = dc_json_model_embed_provider_from_val(provider, &embed->provider);
        if (st != DC_OK) return st;
    }

    yyjson_val* author = yyjson_obj_get(val, "author");
    if (author && !yyjson_is_null(author)) {
        if (!yyjson_is_obj(author)) return DC_ERROR_INVALID_FORMAT;
        embed->has_author = 1;
        st = dc_json_model_embed_author_from_val(author, &embed->author);
        if (st != DC_OK) return st;
    }

    yyjson_val* fields = yyjson_obj_get(val, "fields");
    if (fields && !yyjson_is_null(fields)) {
        if (!yyjson_is_arr(fields)) return DC_ERROR_INVALID_FORMAT;
        size_t idx, max;
        yyjson_val* f_val;
        yyjson_arr_foreach(fields, idx, max, f_val) {
            dc_embed_field_t field;
            // No custom init/free for scalar-ish structs? we might need memsetting
            memset(&field, 0, sizeof(field));
            dc_string_init(&field.name);
            dc_string_init(&field.value);

            st = dc_json_model_embed_field_from_val(f_val, &field);
            if (st != DC_OK) {
                 dc_embed_field_free(&field);
                 return st;
            }
            st = dc_vec_push(&embed->fields, &field);
             if (st != DC_OK) {
                 dc_embed_field_free(&field);
                 return st;
            }
        }
    }

    return DC_OK;
}

dc_status_t dc_json_model_mention_from_val(yyjson_val* val, dc_guild_member_t* member) {
    if (!val || !member) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    /* 1. Parse user fields into member->user */
    dc_status_t st = dc_json_model_user_from_val(val, &member->user);
    if (st != DC_OK) return st;
    member->has_user = 1;

    /* 2. Check for "member" field */
    yyjson_val* partial = yyjson_obj_get(val, "member");
    if (partial && yyjson_is_obj(partial)) {
        /* Parse "member" fields into member */
        /* But dc_json_model_guild_member_from_val expects a full member object with potential user inside. 
           We have partial member fields inside "member" object, but need to populate `member`.
           Let's extract fields manually or modify `dc_json_model_guild_member_from_val`?
           Actually, `dc_json_model_guild_member_from_val` handles `user` if present. 
           Here the partial object does NOT have `user`.
           So we can pass `partial` to `dc_json_model_guild_member_from_val` BUT
           that function might reset fields.
           Let's look at `dc_json_model_guild_member_from_val` impl:
           It parses `roles`, `nick`, etc.
        */
         st = dc_json_model_guild_member_from_val(partial, member);
         /* This assumes `dc_json_model_guild_member_from_val` doesn't overwrite existing user data 
            if `user` field is missing in JSON.
            Usually it *sets* fields.
            Let's invoke it safely.
         */
         if (st != DC_OK) return st;
         /* Preserve the user parsed from the parent mention object. */
         member->has_user = 1;
    }
    
    return DC_OK;
}
