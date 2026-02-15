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

/**
 * @brief Log severity levels.
 */
typedef enum {
    DC_LOG_ERROR = 0, /**< Error conditions that require attention */
    DC_LOG_WARN = 1,  /**< Recoverable warnings */
    DC_LOG_INFO = 2,  /**< Informational events */
    DC_LOG_DEBUG = 3, /**< Debugging details */
    DC_LOG_TRACE = 4  /**< Verbose tracing */
} dc_log_level_t;

/**
 * @brief Logging callback signature.
 * @param level Log severity.
 * @param message Null-terminated message (may be NULL).
 * @param user_data Opaque caller-provided context.
 */
typedef void (*dc_log_callback_t)(dc_log_level_t level, const char* message, void* user_data);

/**
 * @brief Convert log level to uppercase text.
 * @param level Log severity to stringify.
 * @return Static string for @p level, or "UNKNOWN" if out of range.
 */
const char* dc_log_level_string(dc_log_level_t level);

/**
 * @brief Default logging callback implementation.
 * @param level Log severity.
 * @param message Null-terminated message (may be NULL).
 * @param user_data Unused callback context.
 *
 * Writes `"[timestamp] LEVEL: message"` to stderr and flushes output.
 */
void dc_log_default_callback(dc_log_level_t level, const char* message, void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* DC_LOG_H */
