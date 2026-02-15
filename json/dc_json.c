/**
 * @file dc_json.c
 * @brief JSON parsing and serialization using yyjson
 */

#include "dc_json.h"
#include "core/dc_alloc.h"
#include "core/dc_snowflake.h"
#include <yyjson.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static dc_status_t dc_parse_u64_strict(const char* str, uint64_t* out) {
    if (!str || !out) return DC_ERROR_NULL_POINTER;
    if (*str == '\0') return DC_ERROR_PARSE_ERROR;
    uint64_t value = 0;
    const char* p = str;
    while (*p) {
        if (*p < '0' || *p > '9') return DC_ERROR_PARSE_ERROR;
        uint64_t digit = (uint64_t)(*p - '0');
        if (value > (UINT64_MAX - digit) / 10ULL) return DC_ERROR_PARSE_ERROR;
        value = value * 10ULL + digit;
        p++;
    }
    *out = value;
    return DC_OK;
}

static dc_status_t dc_json_read_internal(const char* json_data, size_t json_len,
                                         yyjson_read_flag flags, dc_json_doc_t* doc) {
    if (!json_data || !doc) return DC_ERROR_NULL_POINTER;
    doc->doc = NULL;
    doc->root = NULL;
    if (json_len == 0) return DC_ERROR_INVALID_PARAM;

    yyjson_read_err err;
    doc->doc = yyjson_read_opts((char*)(void*)json_data, json_len, flags, NULL, &err);
    if (!doc->doc) {
        doc->root = NULL;
        return DC_ERROR_JSON;
    }

    doc->root = yyjson_doc_get_root(doc->doc);
    if (!doc->root) {
        yyjson_doc_free(doc->doc);
        doc->doc = NULL;
        return DC_ERROR_JSON;
    }
    return DC_OK;
}

dc_status_t dc_json_parse(const char* json_str, dc_json_doc_t* doc) {
    if (!json_str || !doc) return DC_ERROR_NULL_POINTER;
    size_t len = strlen(json_str);
    return dc_json_read_internal(json_str, len, YYJSON_READ_NOFLAG, doc);
}

dc_status_t dc_json_parse_relaxed(const char* json_str, dc_json_doc_t* doc) {
    if (!json_str || !doc) return DC_ERROR_NULL_POINTER;
    size_t len = strlen(json_str);
    return dc_json_read_internal(json_str, len,
                                 YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS,
                                 doc);
}

dc_status_t dc_json_parse_buffer(const char* json_data, size_t json_len, dc_json_doc_t* doc) {
    return dc_json_read_internal(json_data, json_len, YYJSON_READ_NOFLAG, doc);
}

dc_status_t dc_json_parse_buffer_relaxed(const char* json_data, size_t json_len, dc_json_doc_t* doc) {
    return dc_json_read_internal(json_data, json_len,
                                 YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS,
                                 doc);
}

void dc_json_doc_free(dc_json_doc_t* doc) {
    if (!doc) return;
    if (doc->doc) {
        yyjson_doc_free(doc->doc);
        doc->doc = NULL;
    }
    doc->root = NULL;
}

dc_status_t dc_json_mut_doc_create(dc_json_mut_doc_t* doc) {
    if (!doc) return DC_ERROR_NULL_POINTER;
    doc->doc = NULL;
    doc->root = NULL;
    doc->doc = yyjson_mut_doc_new(NULL);
    if (!doc->doc) return DC_ERROR_OUT_OF_MEMORY;
    
    doc->root = yyjson_mut_obj(doc->doc);
    if (!doc->root) {
        yyjson_mut_doc_free(doc->doc);
        doc->doc = NULL;
        return DC_ERROR_OUT_OF_MEMORY;
    }
    
    yyjson_mut_doc_set_root(doc->doc, doc->root);
    return DC_OK;
}

void dc_json_mut_doc_free(dc_json_mut_doc_t* doc) {
    if (!doc) return;
    if (doc->doc) {
        yyjson_mut_doc_free(doc->doc);
        doc->doc = NULL;
    }
    doc->root = NULL;
}

dc_status_t dc_json_mut_doc_serialize(const dc_json_mut_doc_t* doc, dc_string_t* result) {
    if (!doc || !result) return DC_ERROR_NULL_POINTER;
    if (!doc->doc || !doc->root) return DC_ERROR_INVALID_PARAM;
    
    size_t json_len = 0;
    yyjson_write_err err;
    char* json = yyjson_mut_write_opts(doc->doc, YYJSON_WRITE_PRETTY, NULL, &json_len, &err);
    if (!json) return DC_ERROR_JSON;
    
    dc_string_t tmp;
    dc_status_t st = dc_string_init_with_capacity(&tmp, json_len + 1);
    if (st != DC_OK) {
        free(json);
        return st;
    }
    st = dc_string_set_buffer(&tmp, json, json_len);
    free(json);
    if (st != DC_OK) {
        dc_string_free(&tmp);
        return st;
    }
    dc_string_free(result);
    *result = tmp;
    return st;
}

/* Value access helpers */
dc_status_t dc_json_get_string(yyjson_val* val, const char* key, const char** result) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;
    
    *result = yyjson_get_str(field);
    if (!*result) return DC_ERROR_INVALID_FORMAT;
    return DC_OK;
}

dc_status_t dc_json_get_int64(yyjson_val* val, const char* key, int64_t* result) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (!yyjson_is_int(field)) return DC_ERROR_INVALID_FORMAT;
    
    *result = yyjson_get_sint(field);
    return DC_OK;
}

dc_status_t dc_json_get_uint64(yyjson_val* val, const char* key, uint64_t* result) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (!yyjson_is_uint(field)) return DC_ERROR_INVALID_FORMAT;
    
    *result = yyjson_get_uint(field);
    return DC_OK;
}

dc_status_t dc_json_get_bool(yyjson_val* val, const char* key, int* result) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (!yyjson_is_bool(field)) return DC_ERROR_INVALID_FORMAT;
    
    *result = yyjson_get_bool(field) ? 1 : 0;
    return DC_OK;
}

dc_status_t dc_json_get_double(yyjson_val* val, const char* key, double* result) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (!yyjson_is_num(field)) return DC_ERROR_INVALID_FORMAT;
    
    *result = yyjson_get_num(field);
    return DC_OK;
}

dc_status_t dc_json_get_object(yyjson_val* val, const char* key, yyjson_val** result) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (!yyjson_is_obj(field)) return DC_ERROR_INVALID_FORMAT;
    
    *result = field;
    return DC_OK;
}

dc_status_t dc_json_get_array(yyjson_val* val, const char* key, yyjson_val** result) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (!yyjson_is_arr(field)) return DC_ERROR_INVALID_FORMAT;
    
    *result = field;
    return DC_OK;
}

/* Optional value access helpers */
dc_status_t dc_json_get_string_opt(yyjson_val* val, const char* key, const char** result, const char* default_val) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field || yyjson_is_null(field)) {
        *result = default_val;
        return DC_OK;
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;
    
    *result = yyjson_get_str(field);
    if (!*result) return DC_ERROR_INVALID_FORMAT;
    return DC_OK;
}

dc_status_t dc_json_get_int64_opt(yyjson_val* val, const char* key, int64_t* result, int64_t default_val) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field || yyjson_is_null(field)) {
        *result = default_val;
        return DC_OK;
    }
    if (!yyjson_is_int(field)) return DC_ERROR_INVALID_FORMAT;
    
    *result = yyjson_get_sint(field);
    return DC_OK;
}

dc_status_t dc_json_get_uint64_opt(yyjson_val* val, const char* key, uint64_t* result, uint64_t default_val) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field || yyjson_is_null(field)) {
        *result = default_val;
        return DC_OK;
    }
    if (!yyjson_is_uint(field)) return DC_ERROR_INVALID_FORMAT;
    
    *result = yyjson_get_uint(field);
    return DC_OK;
}

dc_status_t dc_json_get_bool_opt(yyjson_val* val, const char* key, int* result, int default_val) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field || yyjson_is_null(field)) {
        *result = default_val;
        return DC_OK;
    }
    if (!yyjson_is_bool(field)) return DC_ERROR_INVALID_FORMAT;
    
    *result = yyjson_get_bool(field) ? 1 : 0;
    return DC_OK;
}

dc_status_t dc_json_get_double_opt(yyjson_val* val, const char* key, double* result, double default_val) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field || yyjson_is_null(field)) {
        *result = default_val;
        return DC_OK;
    }
    if (!yyjson_is_num(field)) return DC_ERROR_INVALID_FORMAT;

    *result = yyjson_get_num(field);
    return DC_OK;
}

dc_status_t dc_json_get_object_opt(yyjson_val* val, const char* key, yyjson_val** result) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field || yyjson_is_null(field)) {
        *result = NULL;
        return DC_OK;
    }
    if (!yyjson_is_obj(field)) return DC_ERROR_INVALID_FORMAT;

    *result = field;
    return DC_OK;
}

dc_status_t dc_json_get_array_opt(yyjson_val* val, const char* key, yyjson_val** result) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field || yyjson_is_null(field)) {
        *result = NULL;
        return DC_OK;
    }
    if (!yyjson_is_arr(field)) return DC_ERROR_INVALID_FORMAT;

    *result = field;
    return DC_OK;
}

/* Optional/nullable helpers */
dc_status_t dc_json_get_string_optional(yyjson_val* val, const char* key, dc_optional_cstr_t* out) {
    if (!val || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) {
        out->is_set = 0;
        out->value = NULL;
        return DC_OK;
    }
    if (yyjson_is_null(field)) return DC_ERROR_INVALID_FORMAT;
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_set = 1;
    out->value = yyjson_get_str(field);
    if (!out->value) return DC_ERROR_INVALID_FORMAT;
    return DC_OK;
}

dc_status_t dc_json_get_string_nullable(yyjson_val* val, const char* key, dc_nullable_cstr_t* out) {
    if (!val || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (yyjson_is_null(field)) {
        out->is_null = 1;
        out->value = NULL;
        return DC_OK;
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_null = 0;
    out->value = yyjson_get_str(field);
    if (!out->value) return DC_ERROR_INVALID_FORMAT;
    return DC_OK;
}

dc_status_t dc_json_get_int64_optional(yyjson_val* val, const char* key, dc_optional_i64_t* out) {
    if (!val || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (yyjson_is_null(field)) return DC_ERROR_INVALID_FORMAT;
    if (!yyjson_is_int(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_set = 1;
    out->value = yyjson_get_sint(field);
    return DC_OK;
}

dc_status_t dc_json_get_int64_nullable(yyjson_val* val, const char* key, dc_nullable_i64_t* out) {
    if (!val || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (yyjson_is_null(field)) {
        out->is_null = 1;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_int(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_null = 0;
    out->value = yyjson_get_sint(field);
    return DC_OK;
}

dc_status_t dc_json_get_uint64_optional(yyjson_val* val, const char* key, dc_optional_u64_t* out) {
    if (!val || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (yyjson_is_null(field)) return DC_ERROR_INVALID_FORMAT;
    if (!yyjson_is_uint(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_set = 1;
    out->value = yyjson_get_uint(field);
    return DC_OK;
}

dc_status_t dc_json_get_uint64_nullable(yyjson_val* val, const char* key, dc_nullable_u64_t* out) {
    if (!val || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (yyjson_is_null(field)) {
        out->is_null = 1;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_uint(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_null = 0;
    out->value = yyjson_get_uint(field);
    return DC_OK;
}

dc_status_t dc_json_get_bool_optional(yyjson_val* val, const char* key, dc_optional_int_t* out) {
    if (!val || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (yyjson_is_null(field)) return DC_ERROR_INVALID_FORMAT;
    if (!yyjson_is_bool(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_set = 1;
    out->value = yyjson_get_bool(field) ? 1 : 0;
    return DC_OK;
}

dc_status_t dc_json_get_bool_nullable(yyjson_val* val, const char* key, dc_nullable_int_t* out) {
    if (!val || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (yyjson_is_null(field)) {
        out->is_null = 1;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_bool(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_null = 0;
    out->value = yyjson_get_bool(field) ? 1 : 0;
    return DC_OK;
}

dc_status_t dc_json_get_double_optional(yyjson_val* val, const char* key, dc_optional_double_t* out) {
    if (!val || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) {
        out->is_set = 0;
        out->value = 0.0;
        return DC_OK;
    }
    if (yyjson_is_null(field)) return DC_ERROR_INVALID_FORMAT;
    if (!yyjson_is_num(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_set = 1;
    out->value = yyjson_get_num(field);
    return DC_OK;
}

dc_status_t dc_json_get_double_nullable(yyjson_val* val, const char* key, dc_nullable_double_t* out) {
    if (!val || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (yyjson_is_null(field)) {
        out->is_null = 1;
        out->value = 0.0;
        return DC_OK;
    }
    if (!yyjson_is_num(field)) return DC_ERROR_INVALID_FORMAT;

    out->is_null = 0;
    out->value = yyjson_get_num(field);
    return DC_OK;
}

/* Snowflake helpers */
dc_status_t dc_json_get_snowflake(yyjson_val* val, const char* key, uint64_t* result) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;
    
    const char* str = yyjson_get_str(field);
    if (!str) return DC_ERROR_INVALID_FORMAT;
    
    dc_snowflake_t snowflake;
    dc_status_t st = dc_snowflake_from_string(str, &snowflake);
    if (st != DC_OK) return st;
    
    *result = snowflake;
    return DC_OK;
}

dc_status_t dc_json_get_snowflake_opt(yyjson_val* val, const char* key, uint64_t* result, uint64_t default_val) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;
    
    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field || yyjson_is_null(field)) {
        *result = default_val;
        return DC_OK;
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;
    
    const char* str = yyjson_get_str(field);
    if (!str) return DC_ERROR_INVALID_FORMAT;
    
    dc_snowflake_t snowflake;
    dc_status_t st = dc_snowflake_from_string(str, &snowflake);
    if (st != DC_OK) return st;
    
    *result = snowflake;
    return DC_OK;
}

dc_status_t dc_json_get_snowflake_optional(yyjson_val* val, const char* key, dc_optional_u64_t* out) {
    if (!val || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (yyjson_is_null(field)) return DC_ERROR_INVALID_FORMAT;
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;

    const char* str = yyjson_get_str(field);
    if (!str) return DC_ERROR_INVALID_FORMAT;

    dc_snowflake_t snowflake;
    dc_status_t st = dc_snowflake_from_string(str, &snowflake);
    if (st != DC_OK) return st;

    out->is_set = 1;
    out->value = snowflake;
    return DC_OK;
}

dc_status_t dc_json_get_snowflake_nullable(yyjson_val* val, const char* key, dc_nullable_u64_t* out) {
    if (!val || !key || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (yyjson_is_null(field)) {
        out->is_null = 1;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;

    const char* str = yyjson_get_str(field);
    if (!str) return DC_ERROR_INVALID_FORMAT;

    dc_snowflake_t snowflake;
    dc_status_t st = dc_snowflake_from_string(str, &snowflake);
    if (st != DC_OK) return st;

    out->is_null = 0;
    out->value = snowflake;
    return DC_OK;
}

/* Permission helpers */
dc_status_t dc_json_get_permission(yyjson_val* val, const char* key, uint64_t* result) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field) return DC_ERROR_NOT_FOUND;
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;

    const char* str = yyjson_get_str(field);
    if (!str) return DC_ERROR_INVALID_FORMAT;
    return dc_parse_u64_strict(str, result);
}

dc_status_t dc_json_get_permission_opt(yyjson_val* val, const char* key, uint64_t* result, uint64_t default_val) {
    if (!val || !key || !result) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_obj(val)) return DC_ERROR_INVALID_FORMAT;

    yyjson_val* field = yyjson_obj_get(val, key);
    if (!field || yyjson_is_null(field)) {
        *result = default_val;
        return DC_OK;
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;

    const char* str = yyjson_get_str(field);
    if (!str) return DC_ERROR_INVALID_FORMAT;
    return dc_parse_u64_strict(str, result);
}

/* Mutable value builders */
yyjson_mut_val* dc_json_mut_create_object(dc_json_mut_doc_t* doc) {
    if (!doc || !doc->doc) return NULL;
    return yyjson_mut_obj(doc->doc);
}

yyjson_mut_val* dc_json_mut_create_array(dc_json_mut_doc_t* doc) {
    if (!doc || !doc->doc) return NULL;
    return yyjson_mut_arr(doc->doc);
}

dc_status_t dc_json_mut_set_string(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key, const char* val) {
    if (!doc || !doc->doc || !obj || !key || !val) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;
    return yyjson_mut_obj_add_strcpy(doc->doc, obj, key, val) ? DC_OK : DC_ERROR_INVALID_PARAM;
}

dc_status_t dc_json_mut_set_int64(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key, int64_t val) {
    if (!doc || !doc->doc || !obj || !key) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    yyjson_mut_val* int_val = yyjson_mut_sint(doc->doc, val);
    if (!int_val) return DC_ERROR_OUT_OF_MEMORY;
    yyjson_mut_val* key_val = yyjson_mut_strcpy(doc->doc, key);
    if (!key_val) return DC_ERROR_OUT_OF_MEMORY;
    return yyjson_mut_obj_add(obj, key_val, int_val) ? DC_OK : DC_ERROR_INVALID_PARAM;
}

dc_status_t dc_json_mut_set_uint64(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key, uint64_t val) {
    if (!doc || !doc->doc || !obj || !key) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    yyjson_mut_val* uint_val = yyjson_mut_uint(doc->doc, val);
    if (!uint_val) return DC_ERROR_OUT_OF_MEMORY;
    yyjson_mut_val* key_val = yyjson_mut_strcpy(doc->doc, key);
    if (!key_val) return DC_ERROR_OUT_OF_MEMORY;
    return yyjson_mut_obj_add(obj, key_val, uint_val) ? DC_OK : DC_ERROR_INVALID_PARAM;
}

dc_status_t dc_json_mut_set_bool(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key, int val) {
    if (!doc || !doc->doc || !obj || !key) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    yyjson_mut_val* bool_val = (val != 0) ? yyjson_mut_true(doc->doc) : yyjson_mut_false(doc->doc);
    if (!bool_val) return DC_ERROR_OUT_OF_MEMORY;
    yyjson_mut_val* key_val = yyjson_mut_strcpy(doc->doc, key);
    if (!key_val) return DC_ERROR_OUT_OF_MEMORY;
    return yyjson_mut_obj_add(obj, key_val, bool_val) ? DC_OK : DC_ERROR_INVALID_PARAM;
}

dc_status_t dc_json_mut_set_null(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key) {
    if (!doc || !doc->doc || !obj || !key) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    yyjson_mut_val* null_val = yyjson_mut_null(doc->doc);
    if (!null_val) return DC_ERROR_OUT_OF_MEMORY;
    yyjson_mut_val* key_val = yyjson_mut_strcpy(doc->doc, key);
    if (!key_val) return DC_ERROR_OUT_OF_MEMORY;
    return yyjson_mut_obj_add(obj, key_val, null_val) ? DC_OK : DC_ERROR_INVALID_PARAM;
}

dc_status_t dc_json_mut_set_snowflake(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key, uint64_t val) {
    if (!doc || !doc->doc || !obj || !key) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;
    
    char buf[32];
    dc_status_t st = dc_snowflake_to_cstr(val, buf, sizeof(buf));
    if (st != DC_OK) return st;

    return yyjson_mut_obj_add_strcpy(doc->doc, obj, key, buf) ? DC_OK : DC_ERROR_INVALID_PARAM;
}

dc_status_t dc_json_mut_set_permission(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key, uint64_t val) {
    if (!doc || !doc->doc || !obj || !key) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    char buf[32];
    int ret = snprintf(buf, sizeof(buf), "%llu", (unsigned long long)val);
    if (ret < 0 || (size_t)ret >= sizeof(buf)) return DC_ERROR_BUFFER_TOO_SMALL;
    return yyjson_mut_obj_add_strcpy(doc->doc, obj, key, buf) ? DC_OK : DC_ERROR_INVALID_PARAM;
}

dc_status_t dc_json_mut_add_allowed_mentions(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                             const char* key,
                                             const dc_allowed_mentions_t* mentions) {
    if (!doc || !doc->doc || !obj || !key || !mentions) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;

    int has_parse = mentions->parse_set;
    int has_users = dc_vec_length(&mentions->users) > 0;
    int has_roles = dc_vec_length(&mentions->roles) > 0;
    int has_replied_user = mentions->replied_user_set;
    if (!has_parse && !has_users && !has_roles && !has_replied_user) return DC_OK;

    yyjson_mut_val* am = yyjson_mut_obj_add_obj(doc->doc, obj, key);
    if (!am) return DC_ERROR_OUT_OF_MEMORY;

    if (has_parse) {
        yyjson_mut_val* parse = yyjson_mut_obj_add_arr(doc->doc, am, "parse");
        if (!parse) return DC_ERROR_OUT_OF_MEMORY;
        if (mentions->parse_users) {
            if (!yyjson_mut_arr_add_strcpy(doc->doc, parse, "users")) return DC_ERROR_OUT_OF_MEMORY;
        }
        if (mentions->parse_roles) {
            if (!yyjson_mut_arr_add_strcpy(doc->doc, parse, "roles")) return DC_ERROR_OUT_OF_MEMORY;
        }
        if (mentions->parse_everyone) {
            if (!yyjson_mut_arr_add_strcpy(doc->doc, parse, "everyone")) return DC_ERROR_OUT_OF_MEMORY;
        }
    }

    if (has_users) {
        yyjson_mut_val* users = yyjson_mut_obj_add_arr(doc->doc, am, "users");
        if (!users) return DC_ERROR_OUT_OF_MEMORY;
        for (size_t i = 0; i < dc_vec_length(&mentions->users); i++) {
            const dc_snowflake_t* id = (const dc_snowflake_t*)dc_vec_at(&mentions->users, i);
            if (!id || !dc_snowflake_is_valid(*id)) return DC_ERROR_INVALID_PARAM;
            char buf[32];
            dc_status_t st = dc_snowflake_to_cstr(*id, buf, sizeof(buf));
            if (st != DC_OK) return st;
            if (!yyjson_mut_arr_add_strcpy(doc->doc, users, buf)) return DC_ERROR_OUT_OF_MEMORY;
        }
    }

    if (has_roles) {
        yyjson_mut_val* roles = yyjson_mut_obj_add_arr(doc->doc, am, "roles");
        if (!roles) return DC_ERROR_OUT_OF_MEMORY;
        for (size_t i = 0; i < dc_vec_length(&mentions->roles); i++) {
            const dc_snowflake_t* id = (const dc_snowflake_t*)dc_vec_at(&mentions->roles, i);
            if (!id || !dc_snowflake_is_valid(*id)) return DC_ERROR_INVALID_PARAM;
            char buf[32];
            dc_status_t st = dc_snowflake_to_cstr(*id, buf, sizeof(buf));
            if (st != DC_OK) return st;
            if (!yyjson_mut_arr_add_strcpy(doc->doc, roles, buf)) return DC_ERROR_OUT_OF_MEMORY;
        }
    }

    if (has_replied_user) {
        dc_status_t st = dc_json_mut_set_bool(doc, am, "replied_user", mentions->replied_user);
        if (st != DC_OK) return st;
    }

    return DC_OK;
}

dc_status_t dc_json_mut_add_attachments(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                        const char* key,
                                        const dc_attachment_descriptor_t* attachments,
                                        size_t count) {
    if (!doc || !doc->doc || !obj || !key) return DC_ERROR_NULL_POINTER;
    if (!yyjson_mut_is_obj(obj)) return DC_ERROR_INVALID_PARAM;
    if (!attachments && count > 0) return DC_ERROR_NULL_POINTER;

    if (count == 0 && !attachments) return DC_OK;

    yyjson_mut_val* arr = yyjson_mut_obj_add_arr(doc->doc, obj, key);
    if (!arr) return DC_ERROR_OUT_OF_MEMORY;

    for (size_t i = 0; i < count; i++) {
        const dc_attachment_descriptor_t* att = &attachments[i];
        yyjson_mut_val* item = yyjson_mut_arr_add_obj(doc->doc, arr);
        if (!item) return DC_ERROR_OUT_OF_MEMORY;

        yyjson_mut_val* id_val = yyjson_mut_uint(doc->doc, att->id);
        if (!id_val) return DC_ERROR_OUT_OF_MEMORY;
        yyjson_mut_val* id_key = yyjson_mut_strcpy(doc->doc, "id");
        if (!id_key) return DC_ERROR_OUT_OF_MEMORY;
        if (!yyjson_mut_obj_add(item, id_key, id_val)) return DC_ERROR_INVALID_PARAM;

        if (att->filename && att->filename[0] != '\0') {
            if (!dc_attachment_filename_is_valid(att->filename)) return DC_ERROR_INVALID_PARAM;
            if (!yyjson_mut_obj_add_strcpy(doc->doc, item, "filename", att->filename)) {
                return DC_ERROR_OUT_OF_MEMORY;
            }
        }

        if (att->description && att->description[0] != '\0') {
            if (!yyjson_mut_obj_add_strcpy(doc->doc, item, "description", att->description)) {
                return DC_ERROR_OUT_OF_MEMORY;
            }
        }
    }

    return DC_OK;
}
