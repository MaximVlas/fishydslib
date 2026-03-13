/**
 * @file dc_log.c
 * @brief Logging helpers implementation
 */

#include "dc_log.h"
#include "core/dc_platform.h"
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

void dc_log_default_callback(dc_log_level_t level, const char* message, void* user_data) {
    (void)user_data;
    char ts[32] = {0};
    int have_ts = 0;
    time_t now = time(NULL);
    if (now != (time_t)-1) {
        struct tm tm_buf;
        if (dc_platform_localtime_safe(&now, &tm_buf) || dc_platform_gmtime_safe(&now, &tm_buf)) {
            if (strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf) > 0) {
                have_ts = 1;
                ts[sizeof(ts) - 1] = '\0';
            }
        }
    }

    if (!have_ts) {
        snprintf(ts, sizeof(ts), "%s", "unknown-time");
    }
    const char* level_str = dc_log_level_string(level);
    if (!level_str) {
        level_str = "UNKNOWN";
    }
    const char* msg = message ? message : "";
    size_t msg_len = strlen(msg);
    int ends_with_nl = (msg_len > 0 && msg[msg_len - 1] == '\n');
    fprintf(stderr, "[%s] %s: %s", ts, level_str, msg);
    if (!ends_with_nl) {
        fputc('\n', stderr);
    }
    fflush(stderr);
}
