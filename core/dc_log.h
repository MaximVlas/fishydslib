#ifndef DC_LOG_H
#define DC_LOG_H

/**
 * @file dc_log.h
 * @brief Simple logging helpers
 */

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DC_LOG_ERROR = 0,
    DC_LOG_WARN = 1,
    DC_LOG_INFO = 2,
    DC_LOG_DEBUG = 3,
    DC_LOG_TRACE = 4
} dc_log_level_t;

typedef void (*dc_log_callback_t)(dc_log_level_t level, const char* message, void* user_data);

const char* dc_log_level_string(dc_log_level_t level);

void dc_log_default_callback(dc_log_level_t level, const char* message, void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* DC_LOG_H */
