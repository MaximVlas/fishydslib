#ifndef DC_TIME_H
#define DC_TIME_H

/**
 * @file dc_time.h
 * @brief ISO8601 parse/format helpers with strict validation
 */

#include <time.h>
#include <stdint.h>
#include "dc_status.h"
#include "dc_string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ISO8601 timestamp structure with millisecond precision
 */
typedef struct {
    int year;           /**< Year (1900+) */
    int month;          /**< Month (1-12) */
    int day;            /**< Day (1-31) */
    int hour;           /**< Hour (0-23) */
    int minute;         /**< Minute (0-59) */
    int second;         /**< Second (0-59) */
    int millisecond;    /**< Millisecond (0-999) */
    int utc_offset_minutes; /**< UTC offset in minutes (e.g., -480 for PST) */
    int is_utc;         /**< 1 if UTC (Z suffix), 0 if local offset */
} dc_iso8601_t;

/**
 * @brief Parse ISO8601 timestamp from string
 * @param str ISO8601 string (e.g., "2023-01-01T12:00:00.000Z")
 * @param timestamp Pointer to store parsed timestamp
 * @return DC_OK on success, error code on failure
 * 
 * Supported formats:
 * - YYYY-MM-DDTHH:MM:SSZ
 * - YYYY-MM-DDTHH:MM:SS.sssZ
 * - YYYY-MM-DDTHH:MM:SS+HH:MM
 * - YYYY-MM-DDTHH:MM:SS.sss+HH:MM
 */
dc_status_t dc_iso8601_parse(const char* str, dc_iso8601_t* timestamp);

/**
 * @brief Format ISO8601 timestamp to string
 * @param timestamp Timestamp to format
 * @param str String to store result
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_iso8601_format(const dc_iso8601_t* timestamp, dc_string_t* str);

/**
 * @brief Format ISO8601 timestamp to C string buffer
 * @param timestamp Timestamp to format
 * @param buffer Buffer to store result
 * @param buffer_size Size of buffer
 * @return DC_OK on success, error code on failure
 * 
 * @note Buffer must be at least 32 bytes for full format with milliseconds and timezone
 */
dc_status_t dc_iso8601_format_cstr(const dc_iso8601_t* timestamp, char* buffer, size_t buffer_size);

/**
 * @brief Convert ISO8601 timestamp to Unix timestamp
 * @param timestamp ISO8601 timestamp
 * @param unix_timestamp Pointer to store Unix timestamp (seconds)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_iso8601_to_unix(const dc_iso8601_t* timestamp, time_t* unix_timestamp);

/**
 * @brief Convert ISO8601 timestamp to Unix timestamp with milliseconds
 * @param timestamp ISO8601 timestamp
 * @param unix_timestamp_ms Pointer to store Unix timestamp (milliseconds)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_iso8601_to_unix_ms(const dc_iso8601_t* timestamp, uint64_t* unix_timestamp_ms);

/**
 * @brief Convert Unix timestamp to ISO8601 timestamp (UTC)
 * @param unix_timestamp Unix timestamp (seconds)
 * @param timestamp Pointer to store ISO8601 timestamp
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_iso8601_from_unix(time_t unix_timestamp, dc_iso8601_t* timestamp);

/**
 * @brief Convert Unix timestamp with milliseconds to ISO8601 timestamp (UTC)
 * @param unix_timestamp_ms Unix timestamp (milliseconds)
 * @param timestamp Pointer to store ISO8601 timestamp
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_iso8601_from_unix_ms(uint64_t unix_timestamp_ms, dc_iso8601_t* timestamp);

/**
 * @brief Get current time as ISO8601 timestamp (UTC)
 * @param timestamp Pointer to store current timestamp
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_iso8601_now_utc(dc_iso8601_t* timestamp);

/**
 * @brief Get current time as ISO8601 timestamp (local time)
 * @param timestamp Pointer to store current timestamp
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_iso8601_now_local(dc_iso8601_t* timestamp);

/**
 * @brief Validate ISO8601 timestamp structure
 * @param timestamp Timestamp to validate
 * @return DC_OK if valid, error code if invalid
 */
dc_status_t dc_iso8601_validate(const dc_iso8601_t* timestamp);

/**
 * @brief Compare two ISO8601 timestamps
 * @param a First timestamp
 * @param b Second timestamp
 * @return -1 if a < b, 0 if a == b, 1 if a > b
 */
int dc_iso8601_compare(const dc_iso8601_t* a, const dc_iso8601_t* b);

/**
 * @brief Add seconds to ISO8601 timestamp
 * @param timestamp Timestamp to modify
 * @param seconds Seconds to add (can be negative)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_iso8601_add_seconds(dc_iso8601_t* timestamp, int64_t seconds);

/**
 * @brief Add milliseconds to ISO8601 timestamp
 * @param timestamp Timestamp to modify
 * @param milliseconds Milliseconds to add (can be negative)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_iso8601_add_milliseconds(dc_iso8601_t* timestamp, int64_t milliseconds);

/**
 * @brief Calculate difference between two timestamps in seconds
 * @param a First timestamp
 * @param b Second timestamp
 * @param diff_seconds Pointer to store difference (a - b) in seconds
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_iso8601_diff_seconds(const dc_iso8601_t* a, const dc_iso8601_t* b, int64_t* diff_seconds);

/**
 * @brief Calculate difference between two timestamps in milliseconds
 * @param a First timestamp
 * @param b Second timestamp
 * @param diff_ms Pointer to store difference (a - b) in milliseconds
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_iso8601_diff_milliseconds(const dc_iso8601_t* a, const dc_iso8601_t* b, int64_t* diff_ms);

#ifdef __cplusplus
}
#endif

#endif /* DC_TIME_H */