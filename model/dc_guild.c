/**
 * @file dc_guild.c
 * @brief Guild model implementation
 */

#include "dc_guild.h"
#include "json/dc_json.h"
#include <limits.h>
#include <string.h>
#include <yyjson.h>

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

static dc_status_t dc_guild_parse_from_val(yyjson_val* val, dc_guild_t* guild) {
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

    st = dc_guild_parse_from_val(doc.root, &tmp);
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
