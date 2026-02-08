#ifndef DC_SNOWFLAKE_H
#define DC_SNOWFLAKE_H

/**
 * @file dc_snowflake.h
 * @brief Discord snowflake ID helpers: parse, format, timestamp extraction
 */

#include <stdint.h>
#include <time.h>
#include "dc_status.h"
#include "dc_string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Discord snowflake ID type (64-bit unsigned integer)
 */
typedef uint64_t dc_snowflake_t;

/**
 * @brief Invalid/null snowflake value
 */
#define DC_SNOWFLAKE_NULL 0ULL

/**
 * @brief Discord epoch (January 1, 2015 00:00:00 UTC)
 */
#define DC_DISCORD_EPOCH 1420070400000ULL

/**
 * @brief Parse snowflake from string
 * @param str String representation of snowflake
 * @param snowflake Pointer to store parsed snowflake
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_snowflake_from_string(const char* str, dc_snowflake_t* snowflake);

/**
 * @brief Convert snowflake to string
 * @param snowflake Snowflake to convert
 * @param str String to store result
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_snowflake_to_string(dc_snowflake_t snowflake, dc_string_t* str);

/**
 * @brief Convert snowflake to C string buffer
 * @param snowflake Snowflake to convert
 * @param buffer Buffer to store result
 * @param buffer_size Size of buffer
 * @return DC_OK on success, error code on failure
 * 
 * @note Buffer must be at least 21 bytes (20 digits + null terminator)
 */
dc_status_t dc_snowflake_to_cstr(dc_snowflake_t snowflake, char* buffer, size_t buffer_size);

/**
 * @brief Extract timestamp from snowflake
 * @param snowflake Snowflake to extract from
 * @param timestamp Pointer to store extracted timestamp (milliseconds since Discord epoch)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_snowflake_timestamp(dc_snowflake_t snowflake, uint64_t* timestamp);

/**
 * @brief Extract Unix timestamp from snowflake
 * @param snowflake Snowflake to extract from
 * @param unix_timestamp Pointer to store Unix timestamp (seconds since Unix epoch)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_snowflake_unix_timestamp(dc_snowflake_t snowflake, time_t* unix_timestamp);

/**
 * @brief Extract Unix timestamp with milliseconds from snowflake
 * @param snowflake Snowflake to extract from
 * @param unix_timestamp_ms Pointer to store Unix timestamp (milliseconds since Unix epoch)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_snowflake_unix_timestamp_ms(dc_snowflake_t snowflake, uint64_t* unix_timestamp_ms);

/**
 * @brief Extract worker ID from snowflake
 * @param snowflake Snowflake to extract from
 * @param worker_id Pointer to store worker ID (0-31)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_snowflake_worker_id(dc_snowflake_t snowflake, uint8_t* worker_id);

/**
 * @brief Extract process ID from snowflake
 * @param snowflake Snowflake to extract from
 * @param process_id Pointer to store process ID (0-31)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_snowflake_process_id(dc_snowflake_t snowflake, uint8_t* process_id);

/**
 * @brief Extract increment from snowflake
 * @param snowflake Snowflake to extract from
 * @param increment Pointer to store increment (0-4095)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_snowflake_increment(dc_snowflake_t snowflake, uint16_t* increment);

/**
 * @brief Check if snowflake is valid (non-zero)
 * @param snowflake Snowflake to check
 * @return 1 if valid, 0 if invalid
 */
int dc_snowflake_is_valid(dc_snowflake_t snowflake);

/**
 * @brief Compare two snowflakes
 * @param a First snowflake
 * @param b Second snowflake
 * @return -1 if a < b, 0 if a == b, 1 if a > b
 */
int dc_snowflake_compare(dc_snowflake_t a, dc_snowflake_t b);

/**
 * @brief Generate a snowflake for the current time (for testing purposes)
 * @param worker_id Worker ID (0-31)
 * @param process_id Process ID (0-31)
 * @param increment Increment (0-4095)
 * @param snowflake Pointer to store generated snowflake
 * @return DC_OK on success, error code on failure
 * 
 * @note This is primarily for testing; real snowflakes should come from Discord
 */
dc_status_t dc_snowflake_generate(uint8_t worker_id, uint8_t process_id, 
                                  uint16_t increment, dc_snowflake_t* snowflake);

#ifdef __cplusplus
}
#endif

#endif /* DC_SNOWFLAKE_H */