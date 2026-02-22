#ifndef DC_TIME_H
#define DC_TIME_H

/**
 * @file dc_time.h
 * @brief ISO 8601 timestamp parsing, formatting, conversion, and arithmetic.
 *
 * This header provides a self-contained interface for working with ISO 8601-1:2019
 * combined date-time timestamps at millisecond precision.
 *
 * Supported string format (ISO 8601-1:2019 §5.4 combined representations):
 *   YYYY-MM-DDThh:mm:ss[.sss][Z | ±hh:mm]
 *
 * Where:
 *   YYYY    - four-digit year  (0000-9999)
 *   MM      - two-digit month  (01-12)
 *   DD      - two-digit day    (01-31, calendar-constrained)
 *   T       - literal date/time separator (ISO 8601 §4.3.2)
 *   hh      - two-digit hour   (00-23), 24-hour clock
 *   mm      - two-digit minute (00-59)
 *   ss      - two-digit second (00-59); leap seconds are NOT represented
 *   .sss    - optional millisecond fraction (000-999)
 *   Z       - UTC designator (ISO 8601 §4.2.5.1); semantically equivalent to +00:00
 *   ±hh:mm  - signed UTC offset; valid range ±14:00 (ISO 8601 §4.2.5.2)
 *
 * All time arithmetic and comparison is performed in UTC milliseconds internally.
 * Timestamps with different timezone offsets are handled correctly.
 *
 * Designed for use with the Discord API, which returns timestamps as ISO 8601
 * UTC strings and encodes creation times in Snowflake IDs as Unix milliseconds.
 *
 * Implementation file: dc_time.c
 * Calendar algorithms: Howard Hinnant, "chrono-Compatible Low-Level Date Algorithms"
 *   https://howardhinnant.github.io/date_algorithms.html
 */

#include <time.h>
#include <stdint.h>
#include "dc_status.h"
#include "dc_string.h"

#ifdef __cplusplus
extern "C" {
#endif


/* =========================================================================
 * Data types
 * ========================================================================= */

/**
 * @brief A parsed ISO 8601 timestamp with millisecond precision.
 *
 * Stores all calendar and time fields as they appear in an ISO 8601 string,
 * along with the timezone information. The struct makes no assumption about
 * UTC vs. local time on its own — that is encoded in `is_utc` and
 * `utc_offset_minutes`.
 *
 * All arithmetic and comparison functions (diff, add, compare) normalise to
 * UTC internally, so mixing timestamps with different offsets is safe.
 *
 * Valid field ranges (enforced by dc_iso8601_validate()):
 *   year:               0000-9999
 *   month:              01-12
 *   day:                01 to days_in_month(year, month)
 *   hour:               00-23
 *   minute:             00-59
 *   second:             00-59
 *   millisecond:        000-999
 *   utc_offset_minutes: -840 to +840  (±14:00)
 *   is_utc:             0 or 1
 *
 * Note: `is_utc == 1` always implies `utc_offset_minutes == 0`. A timestamp
 * parsed from "+00:00" will have `is_utc == 0` and `utc_offset_minutes == 0`,
 * which is functionally identical to UTC but distinguished in the struct.
 * dc_iso8601_format_cstr() normalises both to 'Z' on output.
 */
typedef struct {
    int year;                /**< Full calendar year, e.g. 2024.                      */
    int month;               /**< Month of year: 1 (January) through 12 (December).   */
    int day;                 /**< Day of month: 1 through days_in_month(year, month).  */
    int hour;                /**< Hour of day: 0-23 (24-hour clock).                   */
    int minute;              /**< Minute of hour: 0-59.                                */
    int second;              /**< Second of minute: 0-59. Leap seconds not supported.  */
    int millisecond;         /**< Millisecond fraction: 0-999.                         */
    int utc_offset_minutes;  /**< Signed UTC offset in minutes. Examples:
                              *   UTC+5:30 (IST)  →  +330
                              *   UTC-8:00 (PST)  →  -480
                              *   UTC             →     0  */
    int is_utc;              /**< 1 if the timestamp was marked with 'Z' (UTC).
                              *   0 if it carries a numeric ±hh:mm offset.            */
} dc_iso8601_t;


/* =========================================================================
 * Parsing and formatting
 * ========================================================================= */

/**
 * @brief Parse a null-terminated ISO 8601 string into a dc_iso8601_t.
 *
 * Accepts the following formats (ISO 8601-1:2019 §5.4):
 *   "2024-01-15T09:30:00Z"
 *   "2024-01-15T09:30:00.123Z"
 *   "2024-01-15T09:30:00+05:30"
 *   "2024-01-15T09:30:00.123-08:00"
 *
 * Fractional seconds: 1-3 digits are accepted and normalised to milliseconds.
 *   ".1"    → 100 ms
 *   ".12"   → 120 ms
 *   ".123"  → 123 ms
 *   ".1234" → 123 ms  (digits beyond 3 are silently discarded)
 *
 * Trailing characters after the timezone designator are rejected.
 * The parsed timestamp is validated with dc_iso8601_validate() before returning.
 *
 * @param str        Null-terminated ISO 8601 string to parse. Must not be NULL.
 * @param timestamp  Output structure. Must not be NULL.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if str or timestamp is NULL.
 * @return DC_ERROR_INVALID_FORMAT if the string does not match the expected format.
 * @return DC_ERROR_INVALID_PARAM if any parsed field is out of the valid range.
 */
dc_status_t dc_iso8601_parse(const char* str, dc_iso8601_t* timestamp);

/**
 * @brief Format a dc_iso8601_t timestamp into a dc_string_t.
 *
 * Convenience wrapper around dc_iso8601_format_cstr() that writes into a
 * heap-managed dc_string_t. Uses a 64-byte stack buffer internally.
 *
 * Output format:
 *   - Milliseconds are omitted when millisecond == 0.
 *   - Timezone is always 'Z' when is_utc == 1 or utc_offset_minutes == 0.
 *   - A non-zero offset is written as ±hh:mm.
 *
 * @param timestamp  Timestamp to format. Must not be NULL.
 * @param str        Output dc_string_t. Must not be NULL.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if str is NULL.
 * @return DC_ERROR_INVALID_PARAM if the timestamp fails validation.
 */
dc_status_t dc_iso8601_format(const dc_iso8601_t* timestamp, dc_string_t* str);

/**
 * @brief Format a dc_iso8601_t timestamp into a caller-supplied C string buffer.
 *
 * Writes a null-terminated ISO 8601 string into `buffer`. The buffer must be
 * at least 32 bytes to hold the longest possible output:
 *   "YYYY-MM-DDThh:mm:ss.sss+hh:mm\0"  =  30 characters + null terminator.
 *
 * Output rules:
 *   - Milliseconds (.sss) are included only when millisecond != 0.
 *   - Timezone is 'Z' when is_utc == 1 OR utc_offset_minutes == 0.
 *     A "+00:00" offset is normalised to 'Z' on output.
 *   - A non-zero offset is written as ±hh:mm.
 *
 * @param timestamp    Timestamp to format. Must not be NULL.
 * @param buffer       Destination buffer. Must not be NULL.
 * @param buffer_size  Size of the destination buffer in bytes. Must be >= 32.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if timestamp or buffer is NULL.
 * @return DC_ERROR_BUFFER_TOO_SMALL if buffer_size < 32.
 * @return DC_ERROR_INVALID_PARAM if the timestamp fails validation.
 */
dc_status_t dc_iso8601_format_cstr(const dc_iso8601_t* timestamp, char* buffer, size_t buffer_size);


/* =========================================================================
 * Unix timestamp conversion
 * ========================================================================= */

/**
 * @brief Convert a dc_iso8601_t to a POSIX time_t (whole seconds since the
 *        Unix epoch, 1970-01-01T00:00:00Z).
 *
 * The fractional millisecond part is truncated (not rounded).
 * Delegates to dc_iso8601_to_unix_ms() and divides by 1000.
 *
 * @param timestamp       ISO 8601 timestamp to convert. Must not be NULL.
 * @param unix_timestamp  Output: seconds since the Unix epoch. Must not be NULL.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if unix_timestamp is NULL.
 * @return DC_ERROR_INVALID_PARAM if the timestamp is invalid or pre-epoch.
 */
dc_status_t dc_iso8601_to_unix(const dc_iso8601_t* timestamp, time_t* unix_timestamp);

/**
 * @brief Convert a dc_iso8601_t to milliseconds since the Unix epoch
 *        (1970-01-01T00:00:00Z).
 *
 * The result is always UTC-normalised regardless of the timestamp's original
 * timezone offset. For example, "09:00+05:30" and "03:30Z" both produce the
 * same unix millisecond value.
 *
 * Pre-epoch timestamps (before 1970-01-01T00:00:00Z) are rejected because
 * the return type is unsigned. For Discord use, this is never an issue.
 *
 * @param timestamp          ISO 8601 timestamp to convert. Must not be NULL.
 * @param unix_timestamp_ms  Output: milliseconds since the Unix epoch. Must not be NULL.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if timestamp or unix_timestamp_ms is NULL.
 * @return DC_ERROR_INVALID_PARAM if the timestamp is invalid or pre-epoch.
 */
dc_status_t dc_iso8601_to_unix_ms(const dc_iso8601_t* timestamp, uint64_t* unix_timestamp_ms);

/**
 * @brief Convert a POSIX time_t (whole seconds since the Unix epoch) to a
 *        dc_iso8601_t expressed in UTC.
 *
 * The resulting timestamp will have millisecond == 0, is_utc == 1, and
 * utc_offset_minutes == 0.
 *
 * @param unix_timestamp  Seconds since 1970-01-01T00:00:00Z. Must be >= 0.
 * @param timestamp       Output ISO 8601 timestamp. Must not be NULL.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if timestamp is NULL.
 * @return DC_ERROR_INVALID_PARAM if unix_timestamp < 0.
 */
dc_status_t dc_iso8601_from_unix(time_t unix_timestamp, dc_iso8601_t* timestamp);

/**
 * @brief Convert milliseconds since the Unix epoch to a dc_iso8601_t
 *        expressed in UTC.
 *
 * The resulting timestamp will have is_utc == 1 and utc_offset_minutes == 0.
 * This is the inverse of dc_iso8601_to_unix_ms().
 *
 * Useful for converting Discord Snowflake timestamps, which embed Unix
 * milliseconds directly.
 *
 * @param unix_timestamp_ms  Milliseconds since 1970-01-01T00:00:00Z.
 * @param timestamp          Output ISO 8601 timestamp. Must not be NULL.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if timestamp is NULL.
 * @return DC_ERROR_INVALID_PARAM if the resulting fields fail validation.
 */
dc_status_t dc_iso8601_from_unix_ms(uint64_t unix_timestamp_ms, dc_iso8601_t* timestamp);


/* =========================================================================
 * Current time
 * ========================================================================= */

/**
 * @brief Capture the current wall-clock time as a UTC dc_iso8601_t with
 *        millisecond precision.
 *
 * Uses POSIX clock_gettime(CLOCK_REALTIME) internally. CLOCK_REALTIME is
 * guaranteed to be available on all POSIX.1-2008 conforming systems.
 *
 * Note: CLOCK_REALTIME can jump backwards if the system clock is adjusted
 * (e.g. by NTP slew). If you only need a monotonically increasing interval
 * for elapsed time measurement, use CLOCK_MONOTONIC — but that clock cannot
 * be converted to a calendar date.
 *
 * @param timestamp  Output: current UTC time. Must not be NULL.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if timestamp is NULL.
 * @return DC_ERROR_UNKNOWN if clock_gettime() fails.
 */
dc_status_t dc_iso8601_now_utc(dc_iso8601_t* timestamp);

/**
 * @brief Capture the current wall-clock time as a dc_iso8601_t in the
 *        system's local timezone, with second precision.
 *
 * Uses time(NULL), localtime_r(), and gmtime_r() to determine the local
 * time and compute the UTC offset (including DST adjustments).
 *
 * The resulting timestamp has:
 *   - is_utc == 0
 *   - utc_offset_minutes set to the current local UTC offset
 *   - millisecond == 0  (time(NULL) has only second resolution)
 *
 * For Discord API work, prefer dc_iso8601_now_utc(). This function is more
 * useful for human-readable logging in the bot runner's local timezone.
 *
 * @param timestamp  Output: current local time. Must not be NULL.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if timestamp is NULL.
 * @return DC_ERROR_UNKNOWN if localtime_r() or gmtime_r() fails.
 */
dc_status_t dc_iso8601_now_local(dc_iso8601_t* timestamp);


/* =========================================================================
 * Validation, comparison, and arithmetic
 * ========================================================================= */

/**
 * @brief Validate all fields of a dc_iso8601_t against ISO 8601-1:2019 ranges.
 *
 * Checks:
 *   year:               0000-9999
 *   month:              01-12
 *   day:                01 to days_in_month(year, month)  (leap-year aware)
 *   hour:               00-23
 *   minute:             00-59
 *   second:             00-59
 *   millisecond:        000-999
 *   utc_offset_minutes: -840 to +840  (±14:00, per ISO 8601 §4.2.5.2)
 *
 * Called internally by all parse, format, and conversion functions.
 * You generally do not need to call this directly unless constructing a
 * dc_iso8601_t manually.
 *
 * @param timestamp  Timestamp to validate. Must not be NULL.
 * @return DC_OK if all fields are within valid ranges.
 * @return DC_ERROR_NULL_POINTER if timestamp is NULL.
 * @return DC_ERROR_INVALID_PARAM if any field is out of range.
 */
dc_status_t dc_iso8601_validate(const dc_iso8601_t* timestamp);

/**
 * @brief Compare two ISO 8601 timestamps chronologically.
 *
 * Both timestamps are UTC-normalised before comparison, so differing timezone
 * offsets are handled correctly. For example, "09:00+05:30" and "03:30Z"
 * compare as equal (they represent the same instant).
 *
 * On conversion error (e.g. an invalid timestamp field), 0 is returned.
 * To distinguish "equal" from "error", validate both timestamps with
 * dc_iso8601_validate() before calling this function.
 *
 * @param a  First timestamp.
 * @param b  Second timestamp.
 * @return  -1 if a is earlier than b.
 * @return   0 if a and b represent the same instant (or on error).
 * @return   1 if a is later than b.
 */
int dc_iso8601_compare(const dc_iso8601_t* a, const dc_iso8601_t* b);

/**
 * @brief Add (or subtract) a number of whole seconds to a timestamp, in-place.
 *
 * Pass a negative value to subtract. The result is always returned in UTC
 * (is_utc == 1, utc_offset_minutes == 0), since the conversion to Unix time
 * and back normalises the timezone.
 *
 * @param timestamp  Timestamp to modify. Must not be NULL.
 * @param seconds    Seconds to add. May be negative.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if timestamp is NULL.
 * @return DC_ERROR_INVALID_PARAM if the result would be before the Unix epoch.
 */
dc_status_t dc_iso8601_add_seconds(dc_iso8601_t* timestamp, int64_t seconds);

/**
 * @brief Add (or subtract) a number of milliseconds to a timestamp, in-place.
 *
 * Pass a negative value to subtract. The result is always returned in UTC
 * (is_utc == 1, utc_offset_minutes == 0).
 *
 * @param timestamp     Timestamp to modify. Must not be NULL.
 * @param milliseconds  Milliseconds to add. May be negative.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if timestamp is NULL.
 * @return DC_ERROR_INVALID_PARAM if the result would be before the Unix epoch.
 */
dc_status_t dc_iso8601_add_milliseconds(dc_iso8601_t* timestamp, int64_t milliseconds);

/**
 * @brief Compute the signed difference in whole seconds between two timestamps:
 *        result = a - b.
 *
 * Both timestamps are converted to Unix milliseconds and divided by 1000
 * independently before subtracting, so sub-second differences are truncated
 * (not rounded). A difference of 999 ms returns 0 seconds.
 *
 * Timezone offsets are handled correctly via UTC normalisation.
 *
 * @param a             First timestamp (minuend).
 * @param b             Second timestamp (subtrahend).
 * @param diff_seconds  Output: (a - b) in whole seconds. Must not be NULL.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if diff_seconds is NULL.
 * @return Any error from dc_iso8601_to_unix_ms() on invalid input.
 */
dc_status_t dc_iso8601_diff_seconds(const dc_iso8601_t* a, const dc_iso8601_t* b, int64_t* diff_seconds);

/**
 * @brief Compute the signed difference in milliseconds between two timestamps:
 *        result = a - b.
 *
 * Both timestamps are UTC-normalised before subtracting, so timezone offsets
 * are handled correctly.
 *
 * Sign convention:
 *   positive → a is later than b
 *   negative → a is earlier than b
 *   zero     → a and b represent the same instant
 *
 * Useful for Discord rate limit handling, cooldown tracking, and computing
 * the age of messages or events from their ISO 8601 timestamps.
 *
 * @param a        First timestamp (minuend).
 * @param b        Second timestamp (subtrahend).
 * @param diff_ms  Output: (a - b) in milliseconds. Must not be NULL.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if diff_ms is NULL.
 * @return Any error from dc_iso8601_to_unix_ms() on invalid input.
 */
dc_status_t dc_iso8601_diff_milliseconds(const dc_iso8601_t* a, const dc_iso8601_t* b, int64_t* diff_ms);


#ifdef __cplusplus
}
#endif

#endif /* DC_TIME_H */