/**
 * @file dc_env.c
 * @brief Safe environment and dotenv helpers.
 */

#include "core/dc_env.h"
#include "core/dc_alloc.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#define DC_ENV_HAS_POSIX_STAT 1
#else
#if defined(_WIN32)
#include <direct.h>
#endif
#define DC_ENV_HAS_POSIX_STAT 0
#endif

/* Reasonable default for auto-discovery walking up from CWD. */
#define DC_ENV_DEFAULT_MAX_PARENT_TRAVERSAL ((size_t)25)

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

static int dc_env_is_path_sep(int c) {
    return (c == '/' || c == '\\') ? 1 : 0;
}

static int dc_env_is_absolute_path(const char* path) {
    if (!path || path[0] == '\0') return 0;
    if (dc_env_is_path_sep(path[0])) return 1;
    /* Windows drive letter, e.g. C:\ */
    if (isalpha((unsigned char)path[0]) && path[1] == ':' && dc_env_is_path_sep(path[2])) return 1;
    return 0;
}

static const char* dc_env_get_home_dir(void) {
    const char* home = getenv("HOME");
    if (home && home[0] != '\0') return home;
    home = getenv("USERPROFILE");
    if (home && home[0] != '\0') return home;
    return NULL;
}

static dc_status_t dc_env_resolve_path(const char* in_path, dc_string_t* out_path) {
    if (!in_path || !out_path) return DC_ERROR_NULL_POINTER;
    if (in_path[0] == '\0') return DC_ERROR_INVALID_PARAM;

    /* Expand "~" prefix (POSIX style). */
    if (in_path[0] == '~' && (in_path[1] == '\0' || dc_env_is_path_sep(in_path[1]))) {
        const char* home = dc_env_get_home_dir();
        if (!home) return DC_ERROR_NOT_FOUND;
        if (in_path[1] == '\0') return dc_string_set_cstr(out_path, home);

        /* Join HOME + remainder (keep separator in remainder). */
        dc_status_t st = dc_string_set_cstr(out_path, home);
        if (st != DC_OK) return st;
        return dc_string_append_cstr(out_path, in_path + 1);
    }

    return dc_string_set_cstr(out_path, in_path);
}

static dc_status_t dc_env_open_file(const char* path, FILE** out_file) {
    if (!path || !out_file) return DC_ERROR_NULL_POINTER;
    *out_file = NULL;
    FILE* f = fopen(path, "r");
    if (!f) {
        if (errno == EACCES) return DC_ERROR_FORBIDDEN;
        return DC_ERROR_NOT_FOUND;
    }
    *out_file = f;
    return DC_OK;
}

static dc_status_t dc_env_read_line(FILE* file, dc_string_t* out_line) {
    if (!file || !out_line) return DC_ERROR_NULL_POINTER;
    dc_status_t st = dc_string_clear(out_line);
    if (st != DC_OK) return st;

    char chunk[512];
    int got_any = 0;
    while (fgets(chunk, (int)sizeof(chunk), file)) {
        got_any = 1;
        st = dc_string_append_cstr(out_line, chunk);
        if (st != DC_OK) return st;
        size_t n = strlen(chunk);
        if (n > 0 && chunk[n - 1] == '\n') break;
    }
    if (!got_any) return DC_ERROR_NOT_FOUND; /* EOF */

    /* Strip trailing '\n' to make parsing easier. */
    if (out_line->data && out_line->length > 0 && out_line->data[out_line->length - 1] == '\n') {
        out_line->data[out_line->length - 1] = '\0';
        out_line->length--;
    }
    return DC_OK;
}

static int dc_env_parse_assignment(char* line, char** out_name, char** out_value_raw) {
    if (!line || !out_name || !out_value_raw) return 0;
    *out_name = NULL;
    *out_value_raw = NULL;

    char* p = dc_env_ltrim(line);
    if (*p == '\0' || *p == '#') return 0;

    if (strncmp(p, "export", (size_t)6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
        p = dc_env_ltrim(p + 6);
    }

    char* eq = strchr(p, '=');
    if (!eq) return 0;
    *eq = '\0';

    char* name = dc_env_ltrim(p);
    dc_env_rtrim(name);
    if (name[0] == '\0') return 0;

    char* value = eq + 1;
    dc_env_rtrim(value);

    *out_name = name;
    *out_value_raw = value;
    return 1;
}

static dc_status_t dc_env_parse_value_ex(const char* raw, int allow_empty, dc_string_t* out_value) {
    if (!raw || !out_value) return DC_ERROR_NULL_POINTER;

    const char* start = raw;
    while (*start != '\0' && isspace((unsigned char)*start)) start++;
    if (*start == '\0') {
        return allow_empty ? dc_string_set_cstr(out_value, "") : DC_ERROR_NOT_FOUND;
    }

    /* Quoted values: allow trailing whitespace and optional comment. */
    if (*start == '"' || *start == '\'') {
        char quote = *start;
        start++;

        dc_status_t st = dc_string_clear(out_value);
        if (st != DC_OK) return st;

        const char* p = start;
        while (*p != '\0') {
            if (quote == '"' && *p == '\\') {
                p++;
                if (*p == '\0') return DC_ERROR_INVALID_FORMAT;
                char out_ch = *p;
                switch (*p) {
                    case 'n': out_ch = '\n'; break;
                    case 'r': out_ch = '\r'; break;
                    case 't': out_ch = '\t'; break;
                    case '\\': out_ch = '\\'; break;
                    case '"': out_ch = '"'; break;
                    default: break;
                }
                char ch = (char)out_ch;
                st = dc_string_append_buffer(out_value, &ch, (size_t)1);
                if (st != DC_OK) return st;
                p++;
                continue;
            }

            if (*p == quote) {
                /* After closing quote: only whitespace and/or comment are allowed. */
                p++;
                while (*p != '\0' && isspace((unsigned char)*p)) p++;
                if (*p != '\0' && *p != '#') return DC_ERROR_INVALID_FORMAT;
                if (!allow_empty && dc_string_is_empty(out_value)) return DC_ERROR_NOT_FOUND;
                return DC_OK;
            }

            char ch = (char)*p;
            st = dc_string_append_buffer(out_value, &ch, (size_t)1);
            if (st != DC_OK) return st;
            p++;
        }
        return DC_ERROR_INVALID_FORMAT;
    }

    /* Unquoted: trim trailing whitespace and allow inline comments. */
    const char* end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;

    const char* comment = NULL;
    for (const char* p = start; p < end; p++) {
        if (*p == '#' && p > start && isspace((unsigned char)p[-1])) {
            comment = p;
            break;
        }
    }
    if (comment) {
        end = comment;
        while (end > start && isspace((unsigned char)end[-1])) end--;
    }

    size_t len = (size_t)(end - start);
    if (len == 0) return allow_empty ? dc_string_set_cstr(out_value, "") : DC_ERROR_NOT_FOUND;
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

    dc_string_t resolved;
    dc_status_t st = dc_string_init(&resolved);
    if (st != DC_OK) return st;
    st = dc_env_resolve_path(path, &resolved);
    if (st != DC_OK) {
        dc_string_free(&resolved);
        return st;
    }
    const char* resolved_path = dc_string_cstr(&resolved);

    if ((flags & DC_ENV_FLAG_REQUIRE_PRIVATE_FILE) != 0u && !dc_env_is_private_file(resolved_path)) {
        dc_string_free(&resolved);
        return DC_ERROR_FORBIDDEN;
    }

    FILE* file = NULL;
    st = dc_env_open_file(resolved_path, &file);
    if (st != DC_OK) {
        dc_string_free(&resolved);
        return st;
    }

    dc_string_t line;
    st = dc_string_init(&line);
    if (st != DC_OK) {
        fclose(file);
        dc_string_free(&resolved);
        return st;
    }

    dc_status_t found = DC_ERROR_NOT_FOUND;
    while ((st = dc_env_read_line(file, &line)) == DC_OK) {
        char* name = NULL;
        char* value_raw = NULL;
        if (!dc_env_parse_assignment(line.data, &name, &value_raw)) continue;
        if (!dc_env_key_match(name, key)) continue;

        found = dc_env_parse_value_ex(value_raw, 0, out_value);
        break;
    }
    if (st != DC_OK && st != DC_ERROR_NOT_FOUND) found = st;

    dc_string_free(&line);
    fclose(file);
    dc_string_free(&resolved);
    return found;
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

static dc_status_t dc_env_get_cwd(dc_string_t* out_dir) {
    if (!out_dir) return DC_ERROR_NULL_POINTER;

    size_t cap = 256;
    for (;;) {
        char* buf = (char*)dc_alloc(cap);
        if (!buf) return DC_ERROR_OUT_OF_MEMORY;

#if defined(_WIN32)
        char* ret = _getcwd(buf, (int)cap);
#else
        char* ret = getcwd(buf, cap);
#endif
        if (ret) {
            dc_status_t st = dc_string_set_cstr(out_dir, buf);
            dc_free(buf);
            return st;
        }

        int err = errno;
        dc_free(buf);
        if (err == ERANGE) {
            if (cap > ((size_t)1 << 20)) return DC_ERROR_BUFFER_TOO_SMALL;
            cap *= 2;
            continue;
        }
        return DC_ERROR_INVALID_STATE;
    }
}

static void dc_env_strip_trailing_seps(dc_string_t* path) {
    if (!path || !path->data) return;
    while (path->length > 1 && dc_env_is_path_sep(path->data[path->length - 1])) {
        path->data[path->length - 1] = '\0';
        path->length--;
    }
}

static int dc_env_pop_dir(dc_string_t* path) {
    if (!path || !path->data) return 0;
    dc_env_strip_trailing_seps(path);
    if (path->length <= 1) return 0;

    char* last = strrchr(path->data, '/');
    char* last2 = strrchr(path->data, '\\');
    char* sep = last;
    if (!sep || (last2 && last2 > sep)) sep = last2;
    if (!sep) return 0;
    if (sep == path->data) {
        path->data[1] = '\0';
        path->length = 1;
        return 1;
    }
    *sep = '\0';
    path->length = (size_t)(sep - path->data);
    dc_env_strip_trailing_seps(path);
    return 1;
}

static int dc_env_file_exists(const char* path) {
#if DC_ENV_HAS_POSIX_STAT
    struct stat st;
    if (!path) return 0;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode) ? 1 : 0;
#else
    FILE* f = NULL;
    if (!path) return 0;
    f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
#endif
}

static dc_status_t dc_env_find_file_up_internal(const char* start_dir,
                                                const char* filename,
                                                size_t max_depth,
                                                dc_string_t* out_path) {
    if (!filename || !out_path) return DC_ERROR_NULL_POINTER;
    if (filename[0] == '\0') return DC_ERROR_INVALID_PARAM;

    dc_status_t st;
    dc_string_t cur;
    st = dc_string_init(&cur);
    if (st != DC_OK) return st;

    if (start_dir && start_dir[0] != '\0') {
        /* Resolve start_dir relative to CWD if needed. */
        dc_string_t resolved_start;
        st = dc_string_init(&resolved_start);
        if (st != DC_OK) {
            dc_string_free(&cur);
            return st;
        }
        st = dc_env_resolve_path(start_dir, &resolved_start);
        if (st != DC_OK) {
            dc_string_free(&resolved_start);
            dc_string_free(&cur);
            return st;
        }

        if (dc_env_is_absolute_path(dc_string_cstr(&resolved_start))) {
            st = dc_string_set_cstr(&cur, dc_string_cstr(&resolved_start));
        } else {
            dc_string_t cwd;
            st = dc_string_init(&cwd);
            if (st == DC_OK) st = dc_env_get_cwd(&cwd);
            if (st == DC_OK) {
                st = dc_string_printf(&cur, "%s/%s", dc_string_cstr(&cwd), dc_string_cstr(&resolved_start));
            }
            dc_string_free(&cwd);
        }
        dc_string_free(&resolved_start);
        if (st != DC_OK) {
            dc_string_free(&cur);
            return st;
        }
    } else {
        st = dc_env_get_cwd(&cur);
        if (st != DC_OK) {
            dc_string_free(&cur);
            return st;
        }
    }

    dc_env_strip_trailing_seps(&cur);

    dc_string_t candidate;
    st = dc_string_init(&candidate);
    if (st != DC_OK) {
        dc_string_free(&cur);
        return st;
    }

    for (size_t depth = 0; depth <= max_depth; depth++) {
        const char* dir = dc_string_cstr(&cur);
        if (strcmp(dir, "/") == 0 || strcmp(dir, "\\") == 0) {
            st = dc_string_printf(&candidate, "%s%s", dir, filename);
        } else {
            st = dc_string_printf(&candidate, "%s/%s", dir, filename);
        }
        if (st != DC_OK) break;
        if (dc_env_file_exists(dc_string_cstr(&candidate))) {
            st = dc_string_set_cstr(out_path, dc_string_cstr(&candidate));
            break;
        }

        if (!dc_env_pop_dir(&cur)) {
            st = DC_ERROR_NOT_FOUND;
            break;
        }
    }

    dc_string_free(&candidate);
    dc_string_free(&cur);
    return st;
}

dc_status_t dc_env_get_with_dotenv_search(const char* key,
                                         const char* dotenv_filename,
                                         size_t max_depth,
                                         unsigned flags,
                                         dc_string_t* out_value) {
    if (!key || !out_value) return DC_ERROR_NULL_POINTER;
    if (key[0] == '\0') return DC_ERROR_INVALID_PARAM;

    dc_status_t st = dc_env_get_process(key, out_value);
    if (st == DC_OK) return st;

    const char* env_path = getenv("DC_DOTENV_PATH");
    if (env_path && env_path[0] != '\0') {
        st = dc_env_get_from_file(env_path, key, flags, out_value);
        if (st == DC_OK || st == DC_ERROR_FORBIDDEN) return st;
    }

    const char* filename = (dotenv_filename && dotenv_filename[0] != '\0') ? dotenv_filename : ".env";
    dc_string_t found_path;
    st = dc_string_init(&found_path);
    if (st != DC_OK) return st;

    st = dc_env_find_file_up_internal(NULL, filename, max_depth, &found_path);
    if (st != DC_OK) {
        dc_string_free(&found_path);
        return st;
    }

    st = dc_env_get_from_file(dc_string_cstr(&found_path), key, flags, out_value);
    dc_string_free(&found_path);
    return st;
}

dc_status_t dc_env_get_discord_token_auto(unsigned flags, dc_string_t* out_token) {
    if (!out_token) return DC_ERROR_NULL_POINTER;
    return dc_env_get_with_dotenv_search("DISCORD_TOKEN", ".env", DC_ENV_DEFAULT_MAX_PARENT_TRAVERSAL,
                                         flags, out_token);
}

static int dc_env_is_valid_key(const char* key) {
    if (!key || key[0] == '\0') return 0;
    for (const char* p = key; *p != '\0'; p++) {
        if (isspace((unsigned char)*p)) return 0;
        if (*p == '=') return 0;
    }
    return 1;
}

static dc_status_t dc_env_set_process_var(const char* key, const char* value, int overwrite) {
    if (!key || !value) return DC_ERROR_NULL_POINTER;
    if (!dc_env_is_valid_key(key)) return DC_ERROR_INVALID_PARAM;
#if defined(_WIN32)
    (void)overwrite;
    return (_putenv_s(key, value) == 0) ? DC_OK : DC_ERROR_INVALID_FORMAT;
#else
    return (setenv(key, value, overwrite ? 1 : 0) == 0) ? DC_OK : DC_ERROR_INVALID_FORMAT;
#endif
}

dc_status_t dc_env_load_dotenv(const char* dotenv_path,
                               unsigned flags,
                               size_t* out_loaded) {
    if (out_loaded) *out_loaded = 0;

    const char* selected = dotenv_path;
    if (!selected || selected[0] == '\0') {
        const char* env_path = getenv("DC_DOTENV_PATH");
        if (env_path && env_path[0] != '\0') {
            selected = env_path;
        }
    }

    dc_string_t path;
    dc_status_t st = dc_string_init(&path);
    if (st != DC_OK) return st;

    if (selected && selected[0] != '\0') {
        st = dc_env_resolve_path(selected, &path);
        if (st != DC_OK) {
            dc_string_free(&path);
            return st;
        }
    } else {
        st = dc_env_find_file_up_internal(NULL, ".env", DC_ENV_DEFAULT_MAX_PARENT_TRAVERSAL, &path);
        if (st != DC_OK) {
            dc_string_free(&path);
            return st;
        }
    }

    const char* resolved_path = dc_string_cstr(&path);
    if ((flags & DC_ENV_FLAG_REQUIRE_PRIVATE_FILE) != 0u && !dc_env_is_private_file(resolved_path)) {
        dc_string_free(&path);
        return DC_ERROR_FORBIDDEN;
    }

    FILE* file = NULL;
    st = dc_env_open_file(resolved_path, &file);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    dc_string_t line;
    dc_string_t value;
    st = dc_string_init(&line);
    if (st != DC_OK) {
        fclose(file);
        dc_string_free(&path);
        return st;
    }
    st = dc_string_init(&value);
    if (st != DC_OK) {
        dc_string_free(&line);
        fclose(file);
        dc_string_free(&path);
        return st;
    }

    size_t loaded = 0;
    const int overwrite = ((flags & DC_ENV_FLAG_OVERRIDE_EXISTING) != 0u) ? 1 : 0;
    const int allow_empty = ((flags & DC_ENV_FLAG_ALLOW_EMPTY) != 0u) ? 1 : 0;

    while ((st = dc_env_read_line(file, &line)) == DC_OK) {
        char* name = NULL;
        char* value_raw = NULL;
        if (!dc_env_parse_assignment(line.data, &name, &value_raw)) continue;
        if (!dc_env_is_valid_key(name)) continue;

        dc_status_t pv = dc_env_parse_value_ex(value_raw, allow_empty, &value);
        if (pv != DC_OK) {
            if (pv == DC_ERROR_NOT_FOUND) continue;
            st = pv;
            break;
        }

        if (!overwrite && getenv(name) != NULL) continue;

        pv = dc_env_set_process_var(name, dc_string_cstr(&value), overwrite);
        if (pv != DC_OK) {
            st = pv;
            break;
        }
        loaded++;
    }

    dc_string_free(&value);
    dc_string_free(&line);
    fclose(file);
    dc_string_free(&path);

    if (st != DC_OK && st != DC_ERROR_NOT_FOUND) return st;
    if (out_loaded) *out_loaded = loaded;
    return (loaded > 0) ? DC_OK : DC_ERROR_NOT_FOUND;
}

void dc_env_secure_clear_string(dc_string_t* value) {
    if (!value || !value->data) return;
    volatile char* p = (volatile char*)value->data;
    for (size_t i = 0; i < value->capacity; i++) {
        p[i] = '\0';
    }
    dc_string_clear(value);
}
