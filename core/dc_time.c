/**
 * @file dc_time.c
 * @brief ISO8601 time implementation
 */

#include "dc_time.h"
#include <string.h>
#include <limits.h>

static int dc_is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int dc_days_in_month(int year, int month) {
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12) return 0;
    if (month == 2 && dc_is_leap_year(year)) return 29;
    return days[month - 1];
}

static int dc_parse_fixed_int(const char* s, size_t len, int* out) {
    int value = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        value = (value * 10) + (s[i] - '0');
    }
    *out = value;
    return 1;
}

static void dc_write_2(char* out, int value) {
    out[0] = (char)('0' + (value / 10) % 10);
    out[1] = (char)('0' + (value % 10));
}

static void dc_write_3(char* out, int value) {
    out[0] = (char)('0' + (value / 100) % 10);
    out[1] = (char)('0' + (value / 10) % 10);
    out[2] = (char)('0' + (value % 10));
}

static void dc_write_4(char* out, int value) {
    out[0] = (char)('0' + (value / 1000) % 10);
    out[1] = (char)('0' + (value / 100) % 10);
    out[2] = (char)('0' + (value / 10) % 10);
    out[3] = (char)('0' + (value % 10));
}

static int64_t dc_days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const int m_adj = (m > 2) ? (int)m - 3 : (int)m + 9;
    const unsigned doy = (153u * (unsigned)m_adj + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static void dc_civil_from_days(int64_t z, int* y, int* m, int* d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    int y_tmp = (int)yoe + (int)(era * 400);
    const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    const unsigned mp = (5u * doy + 2u) / 153u;
    const unsigned d_tmp = doy - (153u * mp + 2u) / 5u + 1u;
    const int m_tmp = (int)mp + (mp < 10u ? 3 : -9);
    y_tmp += (m_tmp <= 2);
    *y = y_tmp;
    *m = m_tmp;
    *d = (int)d_tmp;
}

static time_t dc_timegm_utc(const struct tm* tm) {
    int year = tm->tm_year + 1900;
    int month = tm->tm_mon + 1;
    int day = tm->tm_mday;
    int64_t days = dc_days_from_civil(year, (unsigned)month, (unsigned)day);
    int64_t secs = days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
    return (time_t)secs;
}

dc_status_t dc_iso8601_parse(const char* str, dc_iso8601_t* timestamp) {
    if (!str || !timestamp) return DC_ERROR_NULL_POINTER;
    size_t len = strlen(str);
    size_t i = 0;

    if (len < 20) return DC_ERROR_INVALID_FORMAT;

    if (!dc_parse_fixed_int(str + i, (size_t)4, &timestamp->year)) return DC_ERROR_INVALID_FORMAT;
    i += 4;
    if (str[i++] != '-') return DC_ERROR_INVALID_FORMAT;
    if (!dc_parse_fixed_int(str + i, (size_t)2, &timestamp->month)) return DC_ERROR_INVALID_FORMAT;
    i += 2;
    if (str[i++] != '-') return DC_ERROR_INVALID_FORMAT;
    if (!dc_parse_fixed_int(str + i, (size_t)2, &timestamp->day)) return DC_ERROR_INVALID_FORMAT;
    i += 2;
    if (str[i++] != 'T') return DC_ERROR_INVALID_FORMAT;
    if (!dc_parse_fixed_int(str + i, (size_t)2, &timestamp->hour)) return DC_ERROR_INVALID_FORMAT;
    i += 2;
    if (str[i++] != ':') return DC_ERROR_INVALID_FORMAT;
    if (!dc_parse_fixed_int(str + i, (size_t)2, &timestamp->minute)) return DC_ERROR_INVALID_FORMAT;
    i += 2;
    if (str[i++] != ':') return DC_ERROR_INVALID_FORMAT;
    if (!dc_parse_fixed_int(str + i, (size_t)2, &timestamp->second)) return DC_ERROR_INVALID_FORMAT;
    i += 2;

    timestamp->millisecond = 0;
    if (i < len && str[i] == '.') {
        i++;
        int ms = 0;
        int digits = 0;
        while (i < len && digits < 3 && str[i] >= '0' && str[i] <= '9') {
            ms = ms * 10 + (str[i] - '0');
            digits++;
            i++;
        }
        if (digits == 0) return DC_ERROR_INVALID_FORMAT;
        while (digits < 3) {
            ms *= 10;
            digits++;
        }
        timestamp->millisecond = ms;
        if (i < len && str[i] >= '0' && str[i] <= '9') {
            return DC_ERROR_INVALID_FORMAT;
        }
    }

    timestamp->is_utc = 0;
    timestamp->utc_offset_minutes = 0;
    if (i >= len) return DC_ERROR_INVALID_FORMAT;

    if (str[i] == 'Z') {
        timestamp->is_utc = 1;
        timestamp->utc_offset_minutes = 0;
        i++;
    } else if (str[i] == '+' || str[i] == '-') {
        int sign = (str[i] == '-') ? -1 : 1;
        i++;
        int off_hour = 0;
        int off_min = 0;
        if (!dc_parse_fixed_int(str + i, (size_t)2, &off_hour)) return DC_ERROR_INVALID_FORMAT;
        i += 2;
        if (str[i++] != ':') return DC_ERROR_INVALID_FORMAT;
        if (!dc_parse_fixed_int(str + i, (size_t)2, &off_min)) return DC_ERROR_INVALID_FORMAT;
        i += 2;
        timestamp->utc_offset_minutes = sign * (off_hour * 60 + off_min);
        timestamp->is_utc = 0;
    } else {
        return DC_ERROR_INVALID_FORMAT;
    }

    if (i != len) return DC_ERROR_INVALID_FORMAT;
    return dc_iso8601_validate(timestamp);
}

dc_status_t dc_iso8601_format(const dc_iso8601_t* timestamp, dc_string_t* str) {
    if (!str) return DC_ERROR_NULL_POINTER;
    char buf[64];
    dc_status_t status = dc_iso8601_format_cstr(timestamp, buf, sizeof(buf));
    if (status != DC_OK) return status;
    return dc_string_set_cstr(str, buf);
}

dc_status_t dc_iso8601_format_cstr(const dc_iso8601_t* timestamp, char* buffer, size_t buffer_size) {
    if (!timestamp || !buffer) return DC_ERROR_NULL_POINTER;
    if (buffer_size < 32) return DC_ERROR_BUFFER_TOO_SMALL;

    dc_status_t status = dc_iso8601_validate(timestamp);
    if (status != DC_OK) return status;

    const int use_ms = timestamp->millisecond != 0;
    char* p = buffer;

    dc_write_4(p, timestamp->year); p += 4;
    *p++ = '-';
    dc_write_2(p, timestamp->month); p += 2;
    *p++ = '-';
    dc_write_2(p, timestamp->day); p += 2;
    *p++ = 'T';
    dc_write_2(p, timestamp->hour); p += 2;
    *p++ = ':';
    dc_write_2(p, timestamp->minute); p += 2;
    *p++ = ':';
    dc_write_2(p, timestamp->second); p += 2;

    if (use_ms) {
        *p++ = '.';
        dc_write_3(p, timestamp->millisecond); p += 3;
    }

    if (timestamp->is_utc || timestamp->utc_offset_minutes == 0) {
        *p++ = 'Z';
    } else {
        int offset = timestamp->utc_offset_minutes;
        char sign = '+';
        if (offset < 0) {
            sign = '-';
            offset = -offset;
        }
        int off_hour = offset / 60;
        int off_min = offset % 60;
        *p++ = sign;
        dc_write_2(p, off_hour); p += 2;
        *p++ = ':';
        dc_write_2(p, off_min); p += 2;
    }

    *p = '\0';
    return DC_OK;
}

dc_status_t dc_iso8601_to_unix(const dc_iso8601_t* timestamp, time_t* unix_timestamp) {
    if (!unix_timestamp) return DC_ERROR_NULL_POINTER;
    uint64_t ms = 0;
    dc_status_t status = dc_iso8601_to_unix_ms(timestamp, &ms);
    if (status != DC_OK) return status;
    *unix_timestamp = (time_t)(ms / 1000);
    return DC_OK;
}

dc_status_t dc_iso8601_to_unix_ms(const dc_iso8601_t* timestamp, uint64_t* unix_timestamp_ms) {
    if (!timestamp || !unix_timestamp_ms) return DC_ERROR_NULL_POINTER;
    dc_status_t status = dc_iso8601_validate(timestamp);
    if (status != DC_OK) return status;

    int64_t days = dc_days_from_civil(timestamp->year, (unsigned)timestamp->month, (unsigned)timestamp->day);
    int64_t seconds = days * 86400 + timestamp->hour * 3600 + timestamp->minute * 60 + timestamp->second;
    int offset = timestamp->is_utc ? 0 : timestamp->utc_offset_minutes;
    seconds -= (int64_t)offset * 60;
    int64_t ms = seconds * 1000 + timestamp->millisecond;
    if (ms < 0) return DC_ERROR_INVALID_PARAM;
    *unix_timestamp_ms = (uint64_t)ms;
    return DC_OK;
}

dc_status_t dc_iso8601_from_unix(time_t unix_timestamp, dc_iso8601_t* timestamp) {
    if (!timestamp) return DC_ERROR_NULL_POINTER;
    if (unix_timestamp < 0) return DC_ERROR_INVALID_PARAM;
    return dc_iso8601_from_unix_ms((uint64_t)unix_timestamp * 1000ULL, timestamp);
}

dc_status_t dc_iso8601_from_unix_ms(uint64_t unix_timestamp_ms, dc_iso8601_t* timestamp) {
    if (!timestamp) return DC_ERROR_NULL_POINTER;
    int64_t total_ms = (int64_t)unix_timestamp_ms;
    int64_t seconds = total_ms / 1000;
    int millisecond = (int)(total_ms % 1000);
    if (millisecond < 0) {
        millisecond += 1000;
        seconds -= 1;
    }

    int64_t days = seconds / 86400;
    int64_t rem = seconds % 86400;
    if (rem < 0) {
        rem += 86400;
        days -= 1;
    }

    int year = 0, month = 0, day = 0;
    dc_civil_from_days(days, &year, &month, &day);

    timestamp->year = year;
    timestamp->month = month;
    timestamp->day = day;
    timestamp->hour = (int)(rem / 3600);
    timestamp->minute = (int)((rem % 3600) / 60);
    timestamp->second = (int)(rem % 60);
    timestamp->millisecond = millisecond;
    timestamp->utc_offset_minutes = 0;
    timestamp->is_utc = 1;
    return dc_iso8601_validate(timestamp);
}

dc_status_t dc_iso8601_now_utc(dc_iso8601_t* timestamp) {
    if (!timestamp) return DC_ERROR_NULL_POINTER;
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return DC_ERROR_UNKNOWN;
    uint64_t ms = (uint64_t)ts.tv_sec * 1000ULL + ((uint64_t)ts.tv_nsec / 1000000ULL);
    return dc_iso8601_from_unix_ms(ms, timestamp);
}

dc_status_t dc_iso8601_now_local(dc_iso8601_t* timestamp) {
    if (!timestamp) return DC_ERROR_NULL_POINTER;
    time_t now = time(NULL);
    struct tm local_tm;
    struct tm gmt_tm;
    if (!localtime_r(&now, &local_tm)) return DC_ERROR_UNKNOWN;
    if (!gmtime_r(&now, &gmt_tm)) return DC_ERROR_UNKNOWN;

    time_t local_epoch = mktime(&local_tm);
    time_t gmt_epoch = dc_timegm_utc(&gmt_tm);
    int offset_minutes = (int)((local_epoch - gmt_epoch) / 60);

    timestamp->year = local_tm.tm_year + 1900;
    timestamp->month = local_tm.tm_mon + 1;
    timestamp->day = local_tm.tm_mday;
    timestamp->hour = local_tm.tm_hour;
    timestamp->minute = local_tm.tm_min;
    timestamp->second = local_tm.tm_sec;
    timestamp->millisecond = 0;
    timestamp->utc_offset_minutes = offset_minutes;
    timestamp->is_utc = 0;
    return dc_iso8601_validate(timestamp);
}

dc_status_t dc_iso8601_validate(const dc_iso8601_t* timestamp) {
    if (!timestamp) return DC_ERROR_NULL_POINTER;
    if (timestamp->year < 0 || timestamp->year > 9999) return DC_ERROR_INVALID_PARAM;
    if (timestamp->month < 1 || timestamp->month > 12) return DC_ERROR_INVALID_PARAM;
    int dim = dc_days_in_month(timestamp->year, timestamp->month);
    if (timestamp->day < 1 || timestamp->day > dim) return DC_ERROR_INVALID_PARAM;
    if (timestamp->hour < 0 || timestamp->hour > 23) return DC_ERROR_INVALID_PARAM;
    if (timestamp->minute < 0 || timestamp->minute > 59) return DC_ERROR_INVALID_PARAM;
    if (timestamp->second < 0 || timestamp->second > 59) return DC_ERROR_INVALID_PARAM;
    if (timestamp->millisecond < 0 || timestamp->millisecond > 999) return DC_ERROR_INVALID_PARAM;
    if (timestamp->utc_offset_minutes < -14 * 60 || timestamp->utc_offset_minutes > 14 * 60) return DC_ERROR_INVALID_PARAM;
    return DC_OK;
}

int dc_iso8601_compare(const dc_iso8601_t* a, const dc_iso8601_t* b) {
    uint64_t a_ms = 0;
    uint64_t b_ms = 0;
    if (dc_iso8601_to_unix_ms(a, &a_ms) != DC_OK) return 0;
    if (dc_iso8601_to_unix_ms(b, &b_ms) != DC_OK) return 0;
    return (a_ms < b_ms) ? -1 : (a_ms > b_ms) ? 1 : 0;
}

dc_status_t dc_iso8601_add_seconds(dc_iso8601_t* timestamp, int64_t seconds) {
    if (!timestamp) return DC_ERROR_NULL_POINTER;
    uint64_t ms = 0;
    dc_status_t status = dc_iso8601_to_unix_ms(timestamp, &ms);
    if (status != DC_OK) return status;
    int64_t new_ms = (int64_t)ms + seconds * 1000;
    if (new_ms < 0) return DC_ERROR_INVALID_PARAM;
    return dc_iso8601_from_unix_ms((uint64_t)new_ms, timestamp);
}

dc_status_t dc_iso8601_add_milliseconds(dc_iso8601_t* timestamp, int64_t milliseconds) {
    if (!timestamp) return DC_ERROR_NULL_POINTER;
    uint64_t ms = 0;
    dc_status_t status = dc_iso8601_to_unix_ms(timestamp, &ms);
    if (status != DC_OK) return status;
    int64_t new_ms = (int64_t)ms + milliseconds;
    if (new_ms < 0) return DC_ERROR_INVALID_PARAM;
    return dc_iso8601_from_unix_ms((uint64_t)new_ms, timestamp);
}

dc_status_t dc_iso8601_diff_seconds(const dc_iso8601_t* a, const dc_iso8601_t* b, int64_t* diff_seconds) {
    if (!diff_seconds) return DC_ERROR_NULL_POINTER;
    uint64_t a_ms = 0;
    uint64_t b_ms = 0;
    dc_status_t status = dc_iso8601_to_unix_ms(a, &a_ms);
    if (status != DC_OK) return status;
    status = dc_iso8601_to_unix_ms(b, &b_ms);
    if (status != DC_OK) return status;
    *diff_seconds = (int64_t)(a_ms / 1000) - (int64_t)(b_ms / 1000);
    return DC_OK;
}

dc_status_t dc_iso8601_diff_milliseconds(const dc_iso8601_t* a, const dc_iso8601_t* b, int64_t* diff_ms) {
    if (!diff_ms) return DC_ERROR_NULL_POINTER;
    uint64_t a_ms = 0;
    uint64_t b_ms = 0;
    dc_status_t status = dc_iso8601_to_unix_ms(a, &a_ms);
    if (status != DC_OK) return status;
    status = dc_iso8601_to_unix_ms(b, &b_ms);
    if (status != DC_OK) return status;
    *diff_ms = (int64_t)a_ms - (int64_t)b_ms;
    return DC_OK;
}
