/**
 * @file dc_log.c
 * @brief Simple logging helpers implementation
 */

#include "dc_log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

const char* dc_log_level_string(dc_log_level_t level) {
    /* Keep stringification branch-light; callers may pass out-of-range values. */
    static const char* const k_levels[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};
    unsigned u = (unsigned)level;
    if (u < (unsigned)(sizeof(k_levels) / sizeof(k_levels[0]))) {
        return k_levels[u];
    }
    return "UNKNOWN";
}

static int dc_log_localtime_safe(const time_t* t, struct tm* out) {
    if (!t || !out) return 0;
#if defined(_WIN32) || defined(__STDC_LIB_EXT1__)
    /* MSVC and C11 Annex K provide localtime_s(result, timer). */
    return localtime_s(out, t) == 0;
#elif defined(_POSIX_VERSION) || defined(__unix__) || defined(__APPLE__)
    return localtime_r(t, out) != NULL;
#else
    /* Best-effort fallback: localtime() may use static storage (not thread-safe). */
    struct tm* tmp = localtime(t);
    if (!tmp) return 0;
    *out = *tmp;
    return 1;
#endif
}

static int dc_log_gmtime_safe(const time_t* t, struct tm* out) {
    if (!t || !out) return 0;
#if defined(_WIN32) || defined(__STDC_LIB_EXT1__)
    return gmtime_s(out, t) == 0;
#elif defined(_POSIX_VERSION) || defined(__unix__) || defined(__APPLE__)
    return gmtime_r(t, out) != NULL;
#else
    struct tm* tmp = gmtime(t);
    if (!tmp) return 0;
    *out = *tmp;
    return 1;
#endif
}

void dc_log_default_callback(dc_log_level_t level, const char* message, void* user_data) {
    (void)user_data;
    char ts[32];
    ts[0] = '\0';

    time_t now = time(NULL);
    int have_ts = 0;
    if (now != (time_t)-1) {
        struct tm tm_buf;
        if (dc_log_localtime_safe(&now, &tm_buf) || dc_log_gmtime_safe(&now, &tm_buf)) {
            /* "YYYY-mm-dd HH:MM:SS" is 19 chars (+NUL); buffer is intentionally larger. */
            if (strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf) != 0) {
                have_ts = 1;
            }
        }
    }
    if (!have_ts) {
        (void)snprintf(ts, sizeof(ts), "unknown-time");
    }

    const char* msg = message ? message : "";
    size_t msg_len = strlen(msg);
    int ends_with_nl = (msg_len > 0 && msg[msg_len - 1] == '\n');

    /* Avoid double-newlines if caller already included a trailing newline. */
    fprintf(stderr, "[%s] %s: %s", ts, dc_log_level_string(level), msg);
    if (!ends_with_nl) {
        fputc('\n', stderr);
    }
    fflush(stderr);
}
