/**
 * @file dc_env.c
 * @brief Safe environment and dotenv helpers.
 */

#include "core/dc_env.h"
#include "core/dc_alloc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#define DC_ENV_HAS_POSIX_STAT 1
#else
#define DC_ENV_HAS_POSIX_STAT 0
#endif

static char* dc_env_ltrim(char* s) {
    if (!s) return s;
    while (*s != '\0' && isspace((unsigned char)*s)) s++;
    return s;
}

static void dc_env_rtrim(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        n--;
    }
}

static int dc_env_is_private_file(const char* path) {
#if DC_ENV_HAS_POSIX_STAT
    struct stat st;
    if (!path) return 0;
    if (stat(path, &st) != 0) return 0;
    return ((st.st_mode & (S_IRWXG | S_IRWXO)) == 0) ? 1 : 0;
#else
    (void)path;
    return 1;
#endif
}

static int dc_env_key_match(const char* lhs, const char* rhs) {
    if (!lhs || !rhs) return 0;
    return strcmp(lhs, rhs) == 0 ? 1 : 0;
}

static dc_status_t dc_env_parse_value(const char* raw, dc_string_t* out_value) {
    if (!raw || !out_value) return DC_ERROR_NULL_POINTER;

    const char* start = raw;
    while (*start != '\0' && isspace((unsigned char)*start)) start++;
    if (*start == '\0') return DC_ERROR_NOT_FOUND;

    size_t len = strlen(start);
    while (len > 0 && isspace((unsigned char)start[len - 1])) len--;
    if (len == 0) return DC_ERROR_NOT_FOUND;

    if ((start[0] == '"' && start[len - 1] == '"') ||
        (start[0] == '\'' && start[len - 1] == '\'')) {
        start++;
        len -= 2;
    }
    if (len == 0) return DC_ERROR_NOT_FOUND;
    return dc_string_set_buffer(out_value, start, len);
}

dc_status_t dc_env_get_process(const char* key, dc_string_t* out_value) {
    if (!key || !out_value) return DC_ERROR_NULL_POINTER;
    if (key[0] == '\0') return DC_ERROR_INVALID_PARAM;

    const char* val = getenv(key);
    if (!val || val[0] == '\0') return DC_ERROR_NOT_FOUND;
    return dc_string_set_cstr(out_value, val);
}

dc_status_t dc_env_get_from_file(const char* path,
                                 const char* key,
                                 unsigned flags,
                                 dc_string_t* out_value) {
    if (!path || !key || !out_value) return DC_ERROR_NULL_POINTER;
    if (path[0] == '\0' || key[0] == '\0') return DC_ERROR_INVALID_PARAM;

    if ((flags & DC_ENV_FLAG_REQUIRE_PRIVATE_FILE) != 0u &&
        !dc_env_is_private_file(path)) {
        return DC_ERROR_FORBIDDEN;
    }

    FILE* file = fopen(path, "r");
    if (!file) return DC_ERROR_NOT_FOUND;

    char line[2048];
    dc_status_t st = DC_ERROR_NOT_FOUND;
    while (fgets(line, (int)sizeof(line), file)) {
        char* p = dc_env_ltrim(line);
        if (*p == '\0' || *p == '\n' || *p == '#') continue;

        if (strncmp(p, "export", (size_t)6) == 0 &&
            (p[6] == ' ' || p[6] == '\t')) {
            p = dc_env_ltrim(p + 6);
        }

        char* eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char* name = dc_env_ltrim(p);
        dc_env_rtrim(name);
        if (!dc_env_key_match(name, key)) continue;

        char* value = eq + 1;
        dc_env_rtrim(value);
        st = dc_env_parse_value(value, out_value);
        break;
    }
    fclose(file);
    return st;
}

dc_status_t dc_env_get_with_fallback(const char* key,
                                     const char* const* paths,
                                     size_t path_count,
                                     unsigned flags,
                                     dc_string_t* out_value) {
    if (!key || !out_value) return DC_ERROR_NULL_POINTER;
    if (key[0] == '\0') return DC_ERROR_INVALID_PARAM;

    dc_status_t st = dc_env_get_process(key, out_value);
    if (st == DC_OK) return st;

    if (!paths || path_count == 0) return DC_ERROR_NOT_FOUND;

    for (size_t i = 0; i < path_count; i++) {
        const char* path = paths[i];
        if (!path || path[0] == '\0') continue;
        st = dc_env_get_from_file(path, key, flags, out_value);
        if (st == DC_OK) return st;
        if (st == DC_ERROR_FORBIDDEN) return st;
    }
    return DC_ERROR_NOT_FOUND;
}

dc_status_t dc_env_get_discord_token(const char* const* paths,
                                     size_t path_count,
                                     unsigned flags,
                                     dc_string_t* out_token) {
    return dc_env_get_with_fallback("DISCORD_TOKEN", paths, path_count, flags, out_token);
}

void dc_env_secure_clear_string(dc_string_t* value) {
    if (!value || !value->data) return;
    volatile char* p = (volatile char*)value->data;
    for (size_t i = 0; i < value->capacity; i++) {
        p[i] = '\0';
    }
    dc_string_clear(value);
}
