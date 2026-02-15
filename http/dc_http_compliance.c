/**
 * @file dc_http_compliance.c
 * @brief HTTP compliance helpers for Discord API
 */

#include "dc_http_compliance.h"
#include "core/dc_alloc.h"
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <yyjson.h>

static int dc_ascii_tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static const char* dc_skip_ws(const char* s) {
    while (s && (*s == ' ' || *s == '\t')) s++;
    return s;
}

static int dc_ct_matches(const char* value, const char* token) {
    if (!value || !token) return 0;
    value = dc_skip_ws(value);
    size_t i = 0;
    while (token[i] != '\0') {
        int a = dc_ascii_tolower((unsigned char)value[i]);
        int b = dc_ascii_tolower((unsigned char)token[i]);
        if (a != b) return 0;
        i++;
    }
    char next = value[i];
    if (next == '\0') return 1;
    if (next == ';') return 1;
    if (next == ' ' || next == '\t') {
        const char* p = dc_skip_ws(value + i);
        return (p && (*p == '\0' || *p == ';'));
    }
    return 0;
}

static int dc_parse_nonnegative_int(const char* value, int* out) {
    if (!value || !out) return 0;
    errno = 0;
    char* end = NULL;
    long parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return 0;
    if (parsed < 0 || parsed > INT_MAX) return 0;
    *out = (int)parsed;
    return 1;
}

int dc_http_is_discord_api_url(const char* url) {
    if (!url) return 0;
    if (strncmp(url, "https://", (size_t)8) != 0) return 0;
    if (strstr(url, "discordapp.com") != NULL) return 0;
    size_t base_len = strlen(DC_DISCORD_API_BASE_URL);
    if (strlen(url) < base_len) return 0;
    if (strncmp(url, DC_DISCORD_API_BASE_URL, base_len) != 0) return 0;
    char next = url[base_len];
    return (next == '\0' || next == '/' || next == '?' || next == '#');
}

dc_status_t dc_http_build_discord_api_url(const char* path, dc_string_t* out) {
    if (!path || !out) return DC_ERROR_NULL_POINTER;

    if (strncmp(path, "http://", (size_t)7) == 0 || strncmp(path, "https://", (size_t)8) == 0) {
        if (!dc_http_is_discord_api_url(path)) return DC_ERROR_INVALID_PARAM;
        return dc_string_set_cstr(out, path);
    }

    if (path[0] == '\0') {
        return dc_string_set_cstr(out, DC_DISCORD_API_BASE_URL);
    }

    if (path[0] == '/') {
        return dc_string_printf(out, "%s%s", DC_DISCORD_API_BASE_URL, path);
    }
    return dc_string_printf(out, "%s/%s", DC_DISCORD_API_BASE_URL, path);
}

dc_status_t dc_http_format_user_agent(const dc_user_agent_t* ua, dc_string_t* out) {
    if (!ua || !out) return DC_ERROR_NULL_POINTER;
    if (!ua->version || !ua->url) return DC_ERROR_INVALID_PARAM;
    if (ua->version[0] == '\0' || ua->url[0] == '\0') return DC_ERROR_INVALID_PARAM;

    dc_status_t st = dc_string_printf(out, "DiscordBot (%s, %s)", ua->url, ua->version);
    if (st != DC_OK) return st;
    if (ua->name && ua->name[0] != '\0') {
        st = dc_string_append_printf(out, " %s", ua->name);
        if (st != DC_OK) return st;
    }
    if (ua->extra && ua->extra[0] != '\0') {
        st = dc_string_append_printf(out, " %s", ua->extra);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

dc_status_t dc_http_format_default_user_agent(dc_string_t* out) {
    dc_user_agent_t ua = {
        .name = DC_HTTP_LIBRARY_NAME,
        .version = DC_HTTP_LIBRARY_VERSION,
        .url = DC_HTTP_LIBRARY_URL,
        .extra = NULL
    };
    return dc_http_format_user_agent(&ua, out);
}

const char* dc_http_content_type_string(dc_http_content_type_t type) {
    switch (type) {
        case DC_HTTP_CONTENT_TYPE_JSON:
            return "application/json";
        case DC_HTTP_CONTENT_TYPE_FORM_URLENCODED:
            return "application/x-www-form-urlencoded";
        case DC_HTTP_CONTENT_TYPE_MULTIPART:
            return "multipart/form-data";
        default:
            return "";
    }
}

int dc_http_content_type_is_allowed(const char* content_type) {
    return dc_ct_matches(content_type, "application/json") ||
           dc_ct_matches(content_type, "application/x-www-form-urlencoded") ||
           dc_ct_matches(content_type, "multipart/form-data");
}

int dc_http_user_agent_is_valid(const char* value) {
    if (!value) return 0;
    const char* prefix = "DiscordBot (";
    size_t prefix_len = strlen(prefix);
    if (strncmp(value, prefix, prefix_len) != 0) return 0;
    const char* p = value + prefix_len;
    const char* comma = strchr(p, ',');
    if (!comma) return 0;
    if (comma == p) return 0;
    if (comma[1] != ' ') return 0;
    const char* ver = comma + 2;
    if (*ver == '\0') return 0;
    const char* end = strchr(ver, ')');
    if (!end || end == ver) return 0;
    /* Optional suffix after ) must start with a single space */
    if (end[1] == '\0') return 1;
    return end[1] == ' ';
}

dc_status_t dc_http_format_auth_header(dc_http_auth_type_t type, const char* token, dc_string_t* out) {
    if (!token || !out) return DC_ERROR_NULL_POINTER;
    if (token[0] == '\0') return DC_ERROR_INVALID_PARAM;

    switch (type) {
        case DC_HTTP_AUTH_BOT:
            return dc_string_printf(out, "Bot %s", token);
        case DC_HTTP_AUTH_BEARER:
            return dc_string_printf(out, "Bearer %s", token);
        default:
            return DC_ERROR_INVALID_PARAM;
    }
}

dc_status_t dc_http_append_query_bool(dc_string_t* query, const char* key, int value,
                                      dc_http_bool_format_t format) {
    if (!query || !key) return DC_ERROR_NULL_POINTER;
    if (key[0] == '\0') return DC_ERROR_INVALID_PARAM;

    const char* val = NULL;
    char buf[2] = {0, 0};
    if (format == DC_HTTP_BOOL_TRUE_FALSE) {
        val = value ? "true" : "false";
    } else if (format == DC_HTTP_BOOL_ONE_ZERO) {
        buf[0] = value ? '1' : '0';
        val = buf;
    } else {
        return DC_ERROR_INVALID_PARAM;
    }

    if (dc_string_length(query) == 0) {
        dc_status_t st = dc_string_append_char(query, (char)'?');
        if (st != DC_OK) return st;
    } else {
        dc_status_t st = dc_string_append_char(query, (char)'&');
        if (st != DC_OK) return st;
    }

    dc_status_t st = dc_string_append_cstr(query, key);
    if (st != DC_OK) return st;
    st = dc_string_append_char(query, (char)'=');
    if (st != DC_OK) return st;
    return dc_string_append_cstr(query, val);
}

dc_status_t dc_http_error_init(dc_http_error_t* err) {
    if (!err) return DC_ERROR_NULL_POINTER;
    err->code = 0;
    dc_status_t st = dc_string_init(&err->message);
    if (st != DC_OK) return st;
    return dc_string_init(&err->errors);
}

void dc_http_error_free(dc_http_error_t* err) {
    if (!err) return;
    dc_string_free(&err->message);
    dc_string_free(&err->errors);
    err->code = 0;
}

dc_status_t dc_http_error_parse(const char* body, size_t body_len, dc_http_error_t* err) {
    if (!body || !err) return DC_ERROR_NULL_POINTER;
    if (body_len == 0) body_len = strlen(body);
    if (body_len == 0) return DC_ERROR_INVALID_FORMAT;

    char* mutable_body = (char*)dc_alloc(body_len + 1);
    if (!mutable_body) return DC_ERROR_OUT_OF_MEMORY;
    memcpy(mutable_body, body, body_len);
    mutable_body[body_len] = '\0';

    yyjson_doc* doc = yyjson_read_opts(mutable_body, body_len, 0, NULL, NULL);
    if (!doc) {
        dc_free(mutable_body);
        return DC_ERROR_JSON;
    }

    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        dc_free(mutable_body);
        return DC_ERROR_INVALID_FORMAT;
    }

    yyjson_val* code_val = yyjson_obj_get(root, "code");
    if (code_val && yyjson_is_int(code_val)) {
        err->code = (int)yyjson_get_int(code_val);
    } else if (code_val) {
        yyjson_doc_free(doc);
        dc_free(mutable_body);
        return DC_ERROR_INVALID_FORMAT;
    } else {
        err->code = 0;
    }

    yyjson_val* msg_val = yyjson_obj_get(root, "message");
    if (!msg_val || !yyjson_is_str(msg_val)) {
        yyjson_doc_free(doc);
        dc_free(mutable_body);
        return DC_ERROR_INVALID_FORMAT;
    }

    const char* msg = yyjson_get_str(msg_val);
    if (!msg) {
        yyjson_doc_free(doc);
        dc_free(mutable_body);
        return DC_ERROR_INVALID_FORMAT;
    }
    if (dc_string_set_cstr(&err->message, msg) != DC_OK) {
        yyjson_doc_free(doc);
        dc_free(mutable_body);
        return DC_ERROR_OUT_OF_MEMORY;
    }

    yyjson_val* errors_val = yyjson_obj_get(root, "errors");
    if (errors_val) {
        size_t json_len = 0;
        yyjson_write_err werr = {0};
        char* json = yyjson_val_write_opts(errors_val, 0, NULL, &json_len, &werr);
        if (!json) {
            yyjson_doc_free(doc);
            dc_free(mutable_body);
            return DC_ERROR_JSON;
        }
        dc_status_t st = dc_string_set_buffer(&err->errors, json, json_len);
        free(json);
        if (st != DC_OK) {
            yyjson_doc_free(doc);
            dc_free(mutable_body);
            return st;
        }
    } else {
        dc_string_clear(&err->errors);
    }

    yyjson_doc_free(doc);
    dc_free(mutable_body);
    return DC_OK;
}

dc_status_t dc_http_validate_json_body(const char* body, size_t body_len) {
    if (!body) return DC_ERROR_NULL_POINTER;
    if (body_len == 0) body_len = strlen(body);
    if (body_len == 0) return DC_ERROR_INVALID_FORMAT;

    char* mutable_body = (char*)dc_alloc(body_len + 1);
    if (!mutable_body) return DC_ERROR_OUT_OF_MEMORY;
    memcpy(mutable_body, body, body_len);
    mutable_body[body_len] = '\0';

    yyjson_doc* doc = yyjson_read_opts(mutable_body, body_len, 0, NULL, NULL);
    if (!doc) {
        dc_free(mutable_body);
        return DC_ERROR_JSON;
    }
    yyjson_doc_free(doc);
    dc_free(mutable_body);
    return DC_OK;
}

dc_status_t dc_http_rate_limit_init(dc_http_rate_limit_t* rl) {
    if (!rl) return DC_ERROR_NULL_POINTER;
    rl->limit = 0;
    rl->remaining = 0;
    rl->reset = 0.0;
    rl->reset_after = 0.0;
    rl->retry_after = 0.0;
    rl->global = 0;
    rl->scope = DC_HTTP_RATE_LIMIT_SCOPE_UNKNOWN;
    return dc_string_init(&rl->bucket);
}

void dc_http_rate_limit_free(dc_http_rate_limit_t* rl) {
    if (!rl) return;
    dc_string_free(&rl->bucket);
    rl->limit = 0;
    rl->remaining = 0;
    rl->reset = 0.0;
    rl->reset_after = 0.0;
    rl->retry_after = 0.0;
    rl->global = 0;
    rl->scope = DC_HTTP_RATE_LIMIT_SCOPE_UNKNOWN;
}

static dc_http_rate_limit_scope_t dc_http_rate_limit_scope_from_str(const char* value) {
    if (!value) return DC_HTTP_RATE_LIMIT_SCOPE_UNKNOWN;
    if (strcmp(value, "user") == 0) return DC_HTTP_RATE_LIMIT_SCOPE_USER;
    if (strcmp(value, "global") == 0) return DC_HTTP_RATE_LIMIT_SCOPE_GLOBAL;
    if (strcmp(value, "shared") == 0) return DC_HTTP_RATE_LIMIT_SCOPE_SHARED;
    return DC_HTTP_RATE_LIMIT_SCOPE_UNKNOWN;
}

dc_status_t dc_http_rate_limit_parse(
    dc_status_t (*get_header)(void* userdata, const char* name, const char** value),
    void* userdata,
    dc_http_rate_limit_t* rl) {
    
    if (!get_header || !rl) return DC_ERROR_NULL_POINTER;

    const char* value = NULL;
    dc_status_t st;

    /* Parse X-RateLimit-Limit */
    st = get_header(userdata, "X-RateLimit-Limit", &value);
    if (st == DC_OK && value) {
        int parsed = 0;
        if (dc_parse_nonnegative_int(value, &parsed)) {
            rl->limit = parsed;
        }
    }

    /* Parse X-RateLimit-Remaining */
    st = get_header(userdata, "X-RateLimit-Remaining", &value);
    if (st == DC_OK && value) {
        int parsed = 0;
        if (dc_parse_nonnegative_int(value, &parsed)) {
            rl->remaining = parsed;
        }
    }

    /* Parse X-RateLimit-Reset */
    st = get_header(userdata, "X-RateLimit-Reset", &value);
    if (st == DC_OK && value) {
        rl->reset = strtod(value, NULL);
    }

    /* Parse X-RateLimit-Reset-After */
    st = get_header(userdata, "X-RateLimit-Reset-After", &value);
    if (st == DC_OK && value) {
        rl->reset_after = strtod(value, NULL);
    }

    /* Parse X-RateLimit-Bucket */
    st = get_header(userdata, "X-RateLimit-Bucket", &value);
    if (st == DC_OK && value) {
        st = dc_string_set_cstr(&rl->bucket, value);
        if (st != DC_OK) return st;
    }

    /* Parse X-RateLimit-Global */
    st = get_header(userdata, "X-RateLimit-Global", &value);
    if (st == DC_OK && value) {
        rl->global = (strcmp(value, "true") == 0) ? 1 : 0;
    }

    /* Parse X-RateLimit-Scope */
    st = get_header(userdata, "X-RateLimit-Scope", &value);
    if (st == DC_OK && value) {
        rl->scope = dc_http_rate_limit_scope_from_str(value);
    }

    /* Parse Retry-After */
    st = get_header(userdata, "Retry-After", &value);
    if (st == DC_OK && value) {
        rl->retry_after = strtod(value, NULL);
    }

    return DC_OK;
}

dc_status_t dc_http_rate_limit_response_init(dc_http_rate_limit_response_t* rl) {
    if (!rl) return DC_ERROR_NULL_POINTER;
    rl->retry_after = 0.0;
    rl->global = 0;
    rl->code = 0;
    return dc_string_init(&rl->message);
}

void dc_http_rate_limit_response_free(dc_http_rate_limit_response_t* rl) {
    if (!rl) return;
    dc_string_free(&rl->message);
    rl->retry_after = 0.0;
    rl->global = 0;
    rl->code = 0;
}

dc_status_t dc_http_rate_limit_response_parse(const char* body, size_t body_len,
                                              dc_http_rate_limit_response_t* rl) {
    if (!body || !rl) return DC_ERROR_NULL_POINTER;
    if (body_len == 0) body_len = strlen(body);
    if (body_len == 0) return DC_ERROR_INVALID_FORMAT;

    char* mutable_body = (char*)dc_alloc(body_len + 1);
    if (!mutable_body) return DC_ERROR_OUT_OF_MEMORY;
    memcpy(mutable_body, body, body_len);
    mutable_body[body_len] = '\0';

    yyjson_doc* doc = yyjson_read_opts(mutable_body, body_len, 0, NULL, NULL);
    if (!doc) {
        dc_free(mutable_body);
        return DC_ERROR_JSON;
    }

    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        dc_free(mutable_body);
        return DC_ERROR_INVALID_FORMAT;
    }

    yyjson_val* msg_val = yyjson_obj_get(root, "message");
    if (!msg_val || !yyjson_is_str(msg_val)) {
        yyjson_doc_free(doc);
        dc_free(mutable_body);
        return DC_ERROR_INVALID_FORMAT;
    }
    const char* msg = yyjson_get_str(msg_val);
    if (!msg) {
        yyjson_doc_free(doc);
        dc_free(mutable_body);
        return DC_ERROR_INVALID_FORMAT;
    }
    if (dc_string_set_cstr(&rl->message, msg) != DC_OK) {
        yyjson_doc_free(doc);
        dc_free(mutable_body);
        return DC_ERROR_OUT_OF_MEMORY;
    }

    yyjson_val* retry_val = yyjson_obj_get(root, "retry_after");
    if (retry_val && yyjson_is_num(retry_val)) {
        rl->retry_after = yyjson_get_num(retry_val);
    } else if (retry_val) {
        yyjson_doc_free(doc);
        dc_free(mutable_body);
        return DC_ERROR_INVALID_FORMAT;
    }

    yyjson_val* global_val = yyjson_obj_get(root, "global");
    if (global_val && yyjson_is_bool(global_val)) {
        rl->global = yyjson_get_bool(global_val) ? 1 : 0;
    } else if (global_val) {
        yyjson_doc_free(doc);
        dc_free(mutable_body);
        return DC_ERROR_INVALID_FORMAT;
    }

    yyjson_val* code_val = yyjson_obj_get(root, "code");
    if (code_val && yyjson_is_int(code_val)) {
        rl->code = (int)yyjson_get_int(code_val);
    } else if (code_val) {
        yyjson_doc_free(doc);
        dc_free(mutable_body);
        return DC_ERROR_INVALID_FORMAT;
    }

    yyjson_doc_free(doc);
    dc_free(mutable_body);
    return DC_OK;
}
