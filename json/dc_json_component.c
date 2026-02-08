/**
 * @file dc_json_component.c
 * @brief JSON parsing/building for Discord components (v10, V1 + V2)
 */

#include "dc_json_model.h"
#include "core/dc_alloc.h"
#include <limits.h>
#include <string.h>
#include <yyjson.h>

static dc_status_t dc_json_copy_cstr(dc_string_t* dst, const char* src) {
    if (!dst) return DC_ERROR_NULL_POINTER;
    return dc_string_set_cstr(dst, src ? src : "");
}

static dc_status_t dc_int64_to_i32_checked(int64_t val, int32_t* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (val < INT32_MIN || val > INT32_MAX) return DC_ERROR_INVALID_FORMAT;
    *out = (int32_t)val;
    return DC_OK;
}

static dc_status_t dc_int64_to_int_checked(int64_t val, int* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (val < INT_MIN || val > INT_MAX) return DC_ERROR_INVALID_FORMAT;
    *out = (int)val;
    return DC_OK;
}

static dc_status_t dc_json_get_optional_i32_field(yyjson_val* obj, const char* key,
                                                   dc_optional_i32_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        dc_optional_i32_clear(out);
        return DC_OK;
    }
    if (!yyjson_is_int(field)) return DC_ERROR_INVALID_FORMAT;
    int64_t v = yyjson_get_sint(field);
    int32_t checked = 0;
    dc_status_t st = dc_int64_to_i32_checked(v, &checked);
    if (st != DC_OK) return st;
    out->is_set = 1;
    out->value = checked;
    return DC_OK;
}

static dc_status_t dc_json_get_optional_bool_field(yyjson_val* obj, const char* key,
                                                    dc_optional_bool_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        dc_optional_bool_clear(out);
        return DC_OK;
    }
    if (!yyjson_is_bool(field)) return DC_ERROR_INVALID_FORMAT;
    out->is_set = 1;
    out->value = yyjson_get_bool(field) ? 1 : 0;
    return DC_OK;
}

static dc_status_t dc_json_get_optional_string_field(yyjson_val* obj, const char* key,
                                                      dc_optional_string_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_set = 0;
        return dc_string_clear(&out->value);
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;
    out->is_set = 1;
    return dc_json_copy_cstr(&out->value, yyjson_get_str(field));
}

static dc_status_t dc_json_get_optional_snowflake_field(yyjson_val* obj, const char* key,
                                                         dc_optional_snowflake_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        dc_optional_snowflake_clear(out);
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

static dc_status_t dc_json_mut_add_optional_i32_field(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                       const char* key, const dc_optional_i32_t* in) {
    if (!doc || !doc->doc || !obj || !key || !in) return DC_ERROR_NULL_POINTER;
    if (!in->is_set) return DC_OK;
    return dc_json_mut_set_int64(doc, obj, key, (int64_t)in->value);
}

static dc_status_t dc_json_mut_add_optional_bool_field(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                        const char* key, const dc_optional_bool_t* in) {
    if (!doc || !doc->doc || !obj || !key || !in) return DC_ERROR_NULL_POINTER;
    if (!in->is_set) return DC_OK;
    return dc_json_mut_set_bool(doc, obj, key, in->value);
}

static dc_status_t dc_json_mut_add_optional_string_field(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                          const char* key, const dc_optional_string_t* in) {
    if (!doc || !doc->doc || !obj || !key || !in) return DC_ERROR_NULL_POINTER;
    if (!in->is_set) return DC_OK;
    return dc_json_mut_set_string(doc, obj, key, dc_string_cstr(&in->value));
}

static dc_status_t dc_json_mut_add_optional_snowflake_field(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                             const char* key,
                                                             const dc_optional_snowflake_t* in) {
    if (!doc || !doc->doc || !obj || !key || !in) return DC_ERROR_NULL_POINTER;
    if (!in->is_set) return DC_OK;
    return dc_json_mut_set_snowflake(doc, obj, key, in->value);
}

static dc_status_t dc_select_default_value_type_from_cstr(const char* s,
                                                          dc_select_default_value_type_t* out) {
    if (!s || !out) return DC_ERROR_NULL_POINTER;
    if (strcmp(s, "user") == 0) {
        *out = DC_SELECT_DEFAULT_VALUE_TYPE_USER;
    } else if (strcmp(s, "role") == 0) {
        *out = DC_SELECT_DEFAULT_VALUE_TYPE_ROLE;
    } else if (strcmp(s, "channel") == 0) {
        *out = DC_SELECT_DEFAULT_VALUE_TYPE_CHANNEL;
    } else {
        return DC_ERROR_INVALID_FORMAT;
    }
    return DC_OK;
}

static const char* dc_select_default_value_type_to_cstr(dc_select_default_value_type_t type) {
    switch (type) {
        case DC_SELECT_DEFAULT_VALUE_TYPE_USER: return "user";
        case DC_SELECT_DEFAULT_VALUE_TYPE_ROLE: return "role";
        case DC_SELECT_DEFAULT_VALUE_TYPE_CHANNEL: return "channel";
        default: return NULL;
    }
}

static dc_status_t dc_json_parse_partial_emoji(yyjson_val* val, dc_partial_emoji_t* emoji) {
    if (!val || !emoji) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_status_t st = dc_json_get_optional_snowflake_field(val, "id", &emoji->id);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_string_field(val, "name", &emoji->name);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_bool_field(val, "animated", &emoji->animated);
    return st;
}

static dc_status_t dc_json_mut_add_partial_emoji(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                  const char* key, const dc_partial_emoji_t* emoji) {
    if (!doc || !doc->doc || !obj || !key || !emoji) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;
    yyjson_mut_val* emoji_obj = yyjson_mut_obj_add_obj(doc->doc, obj, key);
    if (!emoji_obj) return DC_ERROR_OUT_OF_MEMORY;

    dc_status_t st = dc_json_mut_add_optional_snowflake_field(doc, emoji_obj, "id", &emoji->id);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_string_field(doc, emoji_obj, "name", &emoji->name);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_bool_field(doc, emoji_obj, "animated", &emoji->animated);
    return st;
}

static dc_status_t dc_json_parse_optional_partial_emoji_field(yyjson_val* obj, const char* key,
                                                              dc_partial_emoji_t** out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        *out = NULL;
        return DC_OK;
    }
    if (!yyjson_is_obj(field)) return DC_ERROR_INVALID_FORMAT;

    dc_partial_emoji_t* emoji = (dc_partial_emoji_t*)dc_calloc((size_t)1, sizeof(*emoji));
    if (!emoji) return DC_ERROR_OUT_OF_MEMORY;
    dc_status_t st = dc_partial_emoji_init(emoji);
    if (st != DC_OK) {
        dc_free(emoji);
        return st;
    }
    st = dc_json_parse_partial_emoji(field, emoji);
    if (st != DC_OK) {
        dc_partial_emoji_free(emoji);
        dc_free(emoji);
        return st;
    }
    *out = emoji;
    return DC_OK;
}

static dc_status_t dc_json_parse_unfurled_media_item(yyjson_val* val, dc_unfurled_media_item_t* media) {
    if (!val || !media) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    const char* url = NULL;
    dc_status_t st = dc_json_get_string(val, "url", &url);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&media->url, url);
    if (st != DC_OK) return st;

    st = dc_json_get_optional_string_field(val, "proxy_url", &media->proxy_url);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_i32_field(val, "height", &media->height);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_i32_field(val, "width", &media->width);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_string_field(val, "content_type", &media->content_type);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_snowflake_field(val, "attachment_id", &media->attachment_id);
    return st;
}

static dc_status_t dc_json_mut_add_unfurled_media_item(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                        const char* key,
                                                        const dc_unfurled_media_item_t* media) {
    if (!doc || !doc->doc || !obj || !key || !media) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    yyjson_mut_val* media_obj = yyjson_mut_obj_add_obj(doc->doc, obj, key);
    if (!media_obj) return DC_ERROR_OUT_OF_MEMORY;

    dc_status_t st = dc_json_mut_set_string(doc, media_obj, "url", dc_string_cstr(&media->url));
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_string_field(doc, media_obj, "proxy_url", &media->proxy_url);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_i32_field(doc, media_obj, "height", &media->height);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_i32_field(doc, media_obj, "width", &media->width);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_string_field(doc, media_obj, "content_type", &media->content_type);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_snowflake_field(doc, media_obj, "attachment_id", &media->attachment_id);
    return st;
}

static dc_status_t dc_json_parse_optional_component_field(yyjson_val* obj, const char* key,
                                                          dc_component_t** out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        *out = NULL;
        return DC_OK;
    }
    if (!yyjson_is_obj(field)) return DC_ERROR_INVALID_FORMAT;

    dc_component_t* component = (dc_component_t*)dc_calloc((size_t)1, sizeof(*component));
    if (!component) return DC_ERROR_OUT_OF_MEMORY;
    dc_status_t st = dc_component_init(component);
    if (st != DC_OK) {
        dc_free(component);
        return st;
    }
    st = dc_json_model_component_from_val(field, component);
    if (st != DC_OK) {
        dc_component_free(component);
        dc_free(component);
        return st;
    }
    *out = component;
    return DC_OK;
}

static dc_status_t dc_json_parse_optional_media_field(yyjson_val* obj, const char* key,
                                                      dc_unfurled_media_item_t** out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        *out = NULL;
        return DC_OK;
    }
    if (!yyjson_is_obj(field)) return DC_ERROR_INVALID_FORMAT;

    dc_unfurled_media_item_t* media = (dc_unfurled_media_item_t*)dc_calloc((size_t)1, sizeof(*media));
    if (!media) return DC_ERROR_OUT_OF_MEMORY;
    dc_status_t st = dc_unfurled_media_item_init(media);
    if (st != DC_OK) {
        dc_free(media);
        return st;
    }
    st = dc_json_parse_unfurled_media_item(field, media);
    if (st != DC_OK) {
        dc_unfurled_media_item_free(media);
        dc_free(media);
        return st;
    }
    *out = media;
    return DC_OK;
}

static dc_status_t dc_json_parse_select_option(yyjson_val* val, dc_select_option_t* option) {
    if (!val || !option) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    const char* label = NULL;
    const char* value = NULL;
    dc_status_t st = dc_json_get_string(val, "label", &label);
    if (st != DC_OK) return st;
    st = dc_json_get_string(val, "value", &value);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&option->label, label);
    if (st != DC_OK) return st;
    st = dc_json_copy_cstr(&option->value, value);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_string_field(val, "description", &option->description);
    if (st != DC_OK) return st;
    st = dc_json_parse_optional_partial_emoji_field(val, "emoji", &option->emoji);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_bool_field(val, "default", &option->default_val);
    return st;
}

static dc_status_t dc_json_mut_add_select_option(dc_json_mut_doc_t* doc, yyjson_mut_val* arr,
                                                  const dc_select_option_t* option) {
    if (!doc || !doc->doc || !arr || !option) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_arr(arr)) return DC_ERROR_INVALID_PARAM;
    yyjson_mut_val* obj = yyjson_mut_arr_add_obj(doc->doc, arr);
    if (!obj) return DC_ERROR_OUT_OF_MEMORY;

    dc_status_t st = dc_json_mut_set_string(doc, obj, "label", dc_string_cstr(&option->label));
    if (st != DC_OK) return st;
    st = dc_json_mut_set_string(doc, obj, "value", dc_string_cstr(&option->value));
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_string_field(doc, obj, "description", &option->description);
    if (st != DC_OK) return st;
    if (option->emoji) {
        st = dc_json_mut_add_partial_emoji(doc, obj, "emoji", option->emoji);
        if (st != DC_OK) return st;
    }
    st = dc_json_mut_add_optional_bool_field(doc, obj, "default", &option->default_val);
    return st;
}

static dc_status_t dc_json_parse_select_default_value(yyjson_val* val,
                                                       dc_select_default_value_t* default_val) {
    if (!val || !default_val) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    dc_snowflake_t id = 0;
    dc_status_t st = dc_json_get_snowflake(val, "id", &id);
    if (st != DC_OK) return st;
    const char* type = NULL;
    st = dc_json_get_string(val, "type", &type);
    if (st != DC_OK) return st;

    default_val->id = id;
    st = dc_select_default_value_type_from_cstr(type, &default_val->type);
    return st;
}

static dc_status_t dc_json_mut_add_select_default_value(dc_json_mut_doc_t* doc, yyjson_mut_val* arr,
                                                         const dc_select_default_value_t* default_val) {
    if (!doc || !doc->doc || !arr || !default_val) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_arr(arr)) return DC_ERROR_INVALID_PARAM;
    yyjson_mut_val* obj = yyjson_mut_arr_add_obj(doc->doc, arr);
    if (!obj) return DC_ERROR_OUT_OF_MEMORY;

    dc_status_t st = dc_json_mut_set_snowflake(doc, obj, "id", default_val->id);
    if (st != DC_OK) return st;
    const char* type = dc_select_default_value_type_to_cstr(default_val->type);
    if (!type) return DC_ERROR_INVALID_FORMAT;
    st = dc_json_mut_set_string(doc, obj, "type", type);
    return st;
}

static dc_status_t dc_json_parse_select_options_array(yyjson_val* arr, dc_vec_t* out) {
    if (!arr || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_arr(arr)) return DC_ERROR_INVALID_FORMAT;

    yyjson_arr_iter iter = yyjson_arr_iter_with(arr);
    yyjson_val* item = NULL;
    while ((item = yyjson_arr_iter_next(&iter))) {
        dc_select_option_t option;
        dc_status_t st = dc_select_option_init(&option);
        if (st != DC_OK) return st;
        st = dc_json_parse_select_option(item, &option);
        if (st != DC_OK) {
            dc_select_option_free(&option);
            return st;
        }
        st = dc_vec_push(out, &option);
        if (st != DC_OK) {
            dc_select_option_free(&option);
            return st;
        }
    }
    return DC_OK;
}

static dc_status_t dc_json_parse_select_default_values_array(yyjson_val* arr, dc_vec_t* out) {
    if (!arr || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_arr(arr)) return DC_ERROR_INVALID_FORMAT;

    yyjson_arr_iter iter = yyjson_arr_iter_with(arr);
    yyjson_val* item = NULL;
    while ((item = yyjson_arr_iter_next(&iter))) {
        dc_select_default_value_t default_val;
        dc_status_t st = dc_select_default_value_init(&default_val);
        if (st != DC_OK) return st;
        st = dc_json_parse_select_default_value(item, &default_val);
        if (st != DC_OK) {
            dc_select_default_value_free(&default_val);
            return st;
        }
        st = dc_vec_push(out, &default_val);
        if (st != DC_OK) {
            dc_select_default_value_free(&default_val);
            return st;
        }
    }
    return DC_OK;
}

static dc_status_t dc_json_parse_component_array(yyjson_val* arr, dc_vec_t* out) {
    if (!arr || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_arr(arr)) return DC_ERROR_INVALID_FORMAT;

    yyjson_arr_iter iter = yyjson_arr_iter_with(arr);
    yyjson_val* item = NULL;
    while ((item = yyjson_arr_iter_next(&iter))) {
        dc_component_t component;
        dc_status_t st = dc_component_init(&component);
        if (st != DC_OK) return st;
        st = dc_json_model_component_from_val(item, &component);
        if (st != DC_OK) {
            dc_component_free(&component);
            return st;
        }
        st = dc_vec_push(out, &component);
        if (st != DC_OK) {
            dc_component_free(&component);
            return st;
        }
    }
    return DC_OK;
}

static dc_status_t dc_json_parse_channel_types_array(yyjson_val* arr, dc_vec_t* out) {
    if (!arr || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_arr(arr)) return DC_ERROR_INVALID_FORMAT;

    yyjson_arr_iter iter = yyjson_arr_iter_with(arr);
    yyjson_val* item = NULL;
    while ((item = yyjson_arr_iter_next(&iter))) {
        if (!yyjson_is_int(item)) return DC_ERROR_INVALID_FORMAT;
        int64_t v = yyjson_get_sint(item);
        int checked = 0;
        dc_status_t st = dc_int64_to_int_checked(v, &checked);
        if (st != DC_OK) return st;
        st = dc_vec_push(out, &checked);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

static dc_status_t dc_json_parse_media_gallery_item(yyjson_val* val, dc_media_gallery_item_t* item) {
    if (!val || !item) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* media_val = yyjson_obj_get(val, "media");
    if (!media_val || yyjson_is_null(media_val)) return DC_ERROR_NOT_FOUND;
    dc_status_t st = dc_json_parse_unfurled_media_item(media_val, &item->media);
    if (st != DC_OK) return st;

    st = dc_json_get_optional_string_field(val, "description", &item->description);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_bool_field(val, "spoiler", &item->spoiler);
    return st;
}

static dc_status_t dc_json_parse_media_gallery_items_array(yyjson_val* arr, dc_vec_t* out) {
    if (!arr || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_arr(arr)) return DC_ERROR_INVALID_FORMAT;

    yyjson_arr_iter iter = yyjson_arr_iter_with(arr);
    yyjson_val* item = NULL;
    while ((item = yyjson_arr_iter_next(&iter))) {
        dc_media_gallery_item_t gallery_item;
        dc_status_t st = dc_media_gallery_item_init(&gallery_item);
        if (st != DC_OK) return st;
        st = dc_json_parse_media_gallery_item(item, &gallery_item);
        if (st != DC_OK) {
            dc_media_gallery_item_free(&gallery_item);
            return st;
        }
        st = dc_vec_push(out, &gallery_item);
        if (st != DC_OK) {
            dc_media_gallery_item_free(&gallery_item);
            return st;
        }
    }
    return DC_OK;
}

static dc_status_t dc_json_mut_add_component_array(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                    const char* key, const dc_vec_t* components) {
    if (!doc || !doc->doc || !obj || !key || !components) return DC_ERROR_NULL_POINTER;
    if (dc_vec_is_empty(components)) return DC_OK;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    yyjson_mut_val* arr = yyjson_mut_obj_add_arr(doc->doc, obj, key);
    if (!arr) return DC_ERROR_OUT_OF_MEMORY;
    for (size_t i = 0; i < dc_vec_length(components); i++) {
        const dc_component_t* component = dc_vec_at(components, i);
        yyjson_mut_val* child = yyjson_mut_arr_add_obj(doc->doc, arr);
        if (!child) return DC_ERROR_OUT_OF_MEMORY;
        dc_status_t st = dc_json_model_component_to_mut(doc, child, component);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

static dc_status_t dc_json_mut_add_select_options_array(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                         const dc_vec_t* options) {
    if (!doc || !doc->doc || !obj || !options) return DC_ERROR_NULL_POINTER;
    if (dc_vec_is_empty(options)) return DC_OK;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    yyjson_mut_val* arr = yyjson_mut_obj_add_arr(doc->doc, obj, "options");
    if (!arr) return DC_ERROR_OUT_OF_MEMORY;
    for (size_t i = 0; i < dc_vec_length(options); i++) {
        const dc_select_option_t* option = dc_vec_at(options, i);
        dc_status_t st = dc_json_mut_add_select_option(doc, arr, option);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

static dc_status_t dc_json_mut_add_select_default_values_array(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                                const dc_vec_t* default_values) {
    if (!doc || !doc->doc || !obj || !default_values) return DC_ERROR_NULL_POINTER;
    if (dc_vec_is_empty(default_values)) return DC_OK;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    yyjson_mut_val* arr = yyjson_mut_obj_add_arr(doc->doc, obj, "default_values");
    if (!arr) return DC_ERROR_OUT_OF_MEMORY;
    for (size_t i = 0; i < dc_vec_length(default_values); i++) {
        const dc_select_default_value_t* default_val = dc_vec_at(default_values, i);
        dc_status_t st = dc_json_mut_add_select_default_value(doc, arr, default_val);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

static dc_status_t dc_json_mut_add_channel_types_array(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                        const dc_vec_t* channel_types) {
    if (!doc || !doc->doc || !obj || !channel_types) return DC_ERROR_NULL_POINTER;
    if (dc_vec_is_empty(channel_types)) return DC_OK;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    yyjson_mut_val* arr = yyjson_mut_obj_add_arr(doc->doc, obj, "channel_types");
    if (!arr) return DC_ERROR_OUT_OF_MEMORY;
    for (size_t i = 0; i < dc_vec_length(channel_types); i++) {
        const int* channel_type = dc_vec_at(channel_types, i);
        if (!yyjson_mut_arr_add_int(doc->doc, arr, (int64_t)*channel_type)) {
            return DC_ERROR_OUT_OF_MEMORY;
        }
    }
    return DC_OK;
}

static dc_status_t dc_json_mut_add_media_gallery_items_array(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                                              const dc_vec_t* items) {
    if (!doc || !doc->doc || !obj || !items) return DC_ERROR_NULL_POINTER;
    if (dc_vec_is_empty(items)) return DC_OK;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    yyjson_mut_val* arr = yyjson_mut_obj_add_arr(doc->doc, obj, "items");
    if (!arr) return DC_ERROR_OUT_OF_MEMORY;
    for (size_t i = 0; i < dc_vec_length(items); i++) {
        const dc_media_gallery_item_t* item = dc_vec_at(items, i);
        yyjson_mut_val* item_obj = yyjson_mut_arr_add_obj(doc->doc, arr);
        if (!item_obj) return DC_ERROR_OUT_OF_MEMORY;
        dc_status_t st = dc_json_mut_add_unfurled_media_item(doc, item_obj, "media", &item->media);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_optional_string_field(doc, item_obj, "description", &item->description);
        if (st != DC_OK) return st;
        st = dc_json_mut_add_optional_bool_field(doc, item_obj, "spoiler", &item->spoiler);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

dc_status_t dc_json_model_component_from_val(yyjson_val* val, dc_component_t* component) {
    if (!val || !component) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    int64_t type_i64 = 0;
    dc_status_t st = dc_json_get_int64(val, "type", &type_i64);
    if (st != DC_OK) return st;
    int type_int = 0;
    st = dc_int64_to_int_checked(type_i64, &type_int);
    if (st != DC_OK) return st;
    component->type = (dc_component_type_t)type_int;

    st = dc_json_get_optional_i32_field(val, "id", &component->id);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_string_field(val, "custom_id", &component->custom_id);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_i32_field(val, "style", &component->style);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_string_field(val, "label", &component->label);
    if (st != DC_OK) return st;
    st = dc_json_parse_optional_partial_emoji_field(val, "emoji", &component->emoji);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_string_field(val, "url", &component->url);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_snowflake_field(val, "sku_id", &component->sku_id);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_bool_field(val, "disabled", &component->disabled);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_string_field(val, "placeholder", &component->placeholder);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_i32_field(val, "min_values", &component->min_values);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_i32_field(val, "max_values", &component->max_values);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_bool_field(val, "required", &component->required);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_i32_field(val, "min_length", &component->min_length);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_i32_field(val, "max_length", &component->max_length);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_string_field(val, "value", &component->value);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_string_field(val, "description", &component->description);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_bool_field(val, "spoiler", &component->spoiler);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_i32_field(val, "accent_color", &component->accent_color);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_bool_field(val, "divider", &component->divider);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_i32_field(val, "spacing", &component->spacing);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_string_field(val, "content", &component->content);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_i32_field(val, "size", &component->size);
    if (st != DC_OK) return st;
    st = dc_json_get_optional_string_field(val, "name", &component->name);
    if (st != DC_OK) return st;

    yyjson_val* options = yyjson_obj_get(val, "options");
    if (options && !yyjson_is_null(options)) {
        st = dc_json_parse_select_options_array(options, &component->options);
        if (st != DC_OK) return st;
    }

    yyjson_val* default_values = yyjson_obj_get(val, "default_values");
    if (default_values && !yyjson_is_null(default_values)) {
        st = dc_json_parse_select_default_values_array(default_values, &component->default_values);
        if (st != DC_OK) return st;
    }

    yyjson_val* channel_types = yyjson_obj_get(val, "channel_types");
    if (channel_types && !yyjson_is_null(channel_types)) {
        st = dc_json_parse_channel_types_array(channel_types, &component->channel_types);
        if (st != DC_OK) return st;
    }

    yyjson_val* components = yyjson_obj_get(val, "components");
    if (components && !yyjson_is_null(components)) {
        st = dc_json_parse_component_array(components, &component->components);
        if (st != DC_OK) return st;
    }

    yyjson_val* items = yyjson_obj_get(val, "items");
    if (items && !yyjson_is_null(items)) {
        st = dc_json_parse_media_gallery_items_array(items, &component->items);
        if (st != DC_OK) return st;
    }

    st = dc_json_parse_optional_component_field(val, "accessory", &component->accessory);
    if (st != DC_OK) return st;
    st = dc_json_parse_optional_media_field(val, "media", &component->media);
    if (st != DC_OK) return st;
    st = dc_json_parse_optional_component_field(val, "component", &component->component);
    if (st != DC_OK) return st;
    st = dc_json_parse_optional_media_field(val, "file", &component->file);
    return st;
}

dc_status_t dc_json_model_component_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                           const dc_component_t* component) {
    if (!doc || !doc->doc || !obj || !component) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    dc_status_t st = dc_json_mut_set_int64(doc, obj, "type", (int64_t)component->type);
    if (st != DC_OK) return st;

    st = dc_json_mut_add_optional_i32_field(doc, obj, "id", &component->id);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_string_field(doc, obj, "custom_id", &component->custom_id);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_i32_field(doc, obj, "style", &component->style);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_string_field(doc, obj, "label", &component->label);
    if (st != DC_OK) return st;
    if (component->emoji) {
        st = dc_json_mut_add_partial_emoji(doc, obj, "emoji", component->emoji);
        if (st != DC_OK) return st;
    }
    st = dc_json_mut_add_optional_string_field(doc, obj, "url", &component->url);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_snowflake_field(doc, obj, "sku_id", &component->sku_id);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_bool_field(doc, obj, "disabled", &component->disabled);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_string_field(doc, obj, "placeholder", &component->placeholder);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_i32_field(doc, obj, "min_values", &component->min_values);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_i32_field(doc, obj, "max_values", &component->max_values);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_bool_field(doc, obj, "required", &component->required);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_i32_field(doc, obj, "min_length", &component->min_length);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_i32_field(doc, obj, "max_length", &component->max_length);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_string_field(doc, obj, "value", &component->value);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_string_field(doc, obj, "description", &component->description);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_bool_field(doc, obj, "spoiler", &component->spoiler);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_i32_field(doc, obj, "accent_color", &component->accent_color);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_bool_field(doc, obj, "divider", &component->divider);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_i32_field(doc, obj, "spacing", &component->spacing);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_string_field(doc, obj, "content", &component->content);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_i32_field(doc, obj, "size", &component->size);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_optional_string_field(doc, obj, "name", &component->name);
    if (st != DC_OK) return st;

    st = dc_json_mut_add_select_options_array(doc, obj, &component->options);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_select_default_values_array(doc, obj, &component->default_values);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_channel_types_array(doc, obj, &component->channel_types);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_component_array(doc, obj, "components", &component->components);
    if (st != DC_OK) return st;
    st = dc_json_mut_add_media_gallery_items_array(doc, obj, &component->items);
    if (st != DC_OK) return st;

    if (component->accessory) {
        yyjson_mut_val* accessory_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "accessory");
        if (!accessory_obj) return DC_ERROR_OUT_OF_MEMORY;
        st = dc_json_model_component_to_mut(doc, accessory_obj, component->accessory);
        if (st != DC_OK) return st;
    }
    if (component->component) {
        yyjson_mut_val* component_obj = yyjson_mut_obj_add_obj(doc->doc, obj, "component");
        if (!component_obj) return DC_ERROR_OUT_OF_MEMORY;
        st = dc_json_model_component_to_mut(doc, component_obj, component->component);
        if (st != DC_OK) return st;
    }
    if (component->media) {
        st = dc_json_mut_add_unfurled_media_item(doc, obj, "media", component->media);
        if (st != DC_OK) return st;
    }
    if (component->file) {
        st = dc_json_mut_add_unfurled_media_item(doc, obj, "file", component->file);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}
