/**
 * @file dc_log.c
 * @brief Simple logging helpers implementation
 */

#include "dc_log.h"
#include <stdio.h>
#include <time.h>

const char* dc_log_level_string(dc_log_level_t level) {
    switch (level) {
        case DC_LOG_ERROR:
            return "ERROR";
        case DC_LOG_WARN:
            return "WARN";
        case DC_LOG_INFO:
            return "INFO";
        case DC_LOG_DEBUG:
            return "DEBUG";
        case DC_LOG_TRACE:
            return "TRACE";
        default:
            return "UNKNOWN";
    }
}

void dc_log_default_callback(dc_log_level_t level, const char* message, void* user_data) {
    (void)user_data;
    char ts[32];
    time_t now = time(NULL);
    struct tm tm_buf;
    struct tm* tm_ptr = localtime_r(&now, &tm_buf);
    if (tm_ptr) {
        if (strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_ptr) == 0) {
            snprintf(ts, sizeof(ts), "unknown-time");
        }
    } else {
        snprintf(ts, sizeof(ts), "unknown-time");
    }
    fprintf(stderr, "[%s] %s: %s\n", ts, dc_log_level_string(level), message ? message : "");
}
