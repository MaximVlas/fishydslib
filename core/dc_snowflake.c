/**
 * @file dc_snowflake.c
 * @brief Discord snowflake implementation
 */

#include "dc_snowflake.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

dc_status_t dc_snowflake_from_string(const char* str, dc_snowflake_t* snowflake) {
    if (!str || !snowflake) return DC_ERROR_NULL_POINTER;

    if (*str == '\0') {
        return DC_ERROR_PARSE_ERROR;
    }

    uint64_t value = 0;
    const char* p = str;
    while (*p) {
        if (*p < '0' || *p > '9') {
            return DC_ERROR_PARSE_ERROR;
        }
        uint64_t digit = (uint64_t)(*p - '0');
        if (value > (UINT64_MAX - digit) / 10ULL) {
            return DC_ERROR_PARSE_ERROR;
        }
        value = value * 10ULL + digit;
        p++;
    }

    *snowflake = (dc_snowflake_t)value;
    return DC_OK;
}

dc_status_t dc_snowflake_to_cstr(dc_snowflake_t snowflake, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 21) return DC_ERROR_INVALID_PARAM;
    
    int ret = snprintf(buffer, buffer_size, "%llu", (unsigned long long)snowflake);
    if (ret < 0 || (size_t)ret >= buffer_size) {
        return DC_ERROR_BUFFER_TOO_SMALL;
    }
    
    return DC_OK;
}

dc_status_t dc_snowflake_timestamp(dc_snowflake_t snowflake, uint64_t* timestamp) {
    if (!timestamp) return DC_ERROR_NULL_POINTER;
    
    *timestamp = (snowflake >> 22) + DC_DISCORD_EPOCH;
    return DC_OK;
}

int dc_snowflake_is_valid(dc_snowflake_t snowflake) {
    return snowflake != DC_SNOWFLAKE_NULL;
}

dc_status_t dc_snowflake_to_string(dc_snowflake_t snowflake, dc_string_t* str) {
    if (!str) return DC_ERROR_NULL_POINTER;
    char buf[32];
    dc_status_t status = dc_snowflake_to_cstr(snowflake, buf, sizeof(buf));
    if (status != DC_OK) return status;
    return dc_string_set_cstr(str, buf);
}

dc_status_t dc_snowflake_unix_timestamp(dc_snowflake_t snowflake, time_t* unix_timestamp) {
    if (!unix_timestamp) return DC_ERROR_NULL_POINTER;
    uint64_t ts_ms = 0;
    dc_status_t status = dc_snowflake_timestamp(snowflake, &ts_ms);
    if (status != DC_OK) return status;
    *unix_timestamp = (time_t)(ts_ms / 1000);
    return DC_OK;
}

dc_status_t dc_snowflake_unix_timestamp_ms(dc_snowflake_t snowflake, uint64_t* unix_timestamp_ms) {
    if (!unix_timestamp_ms) return DC_ERROR_NULL_POINTER;
    return dc_snowflake_timestamp(snowflake, unix_timestamp_ms);
}

dc_status_t dc_snowflake_worker_id(dc_snowflake_t snowflake, uint8_t* worker_id) {
    if (!worker_id) return DC_ERROR_NULL_POINTER;
    *worker_id = (uint8_t)((snowflake & 0x3E0000ULL) >> 17);
    return DC_OK;
}

dc_status_t dc_snowflake_process_id(dc_snowflake_t snowflake, uint8_t* process_id) {
    if (!process_id) return DC_ERROR_NULL_POINTER;
    *process_id = (uint8_t)((snowflake & 0x1F000ULL) >> 12);
    return DC_OK;
}

dc_status_t dc_snowflake_increment(dc_snowflake_t snowflake, uint16_t* increment) {
    if (!increment) return DC_ERROR_NULL_POINTER;
    *increment = (uint16_t)(snowflake & 0xFFFULL);
    return DC_OK;
}
int dc_snowflake_compare(dc_snowflake_t a, dc_snowflake_t b) { return (a < b) ? -1 : (a > b) ? 1 : 0; }
dc_status_t dc_snowflake_generate(uint8_t worker_id, uint8_t process_id, uint16_t increment, dc_snowflake_t* snowflake) {
    if (!snowflake) return DC_ERROR_NULL_POINTER;
    if (worker_id > 31 || process_id > 31 || increment > 4095) return DC_ERROR_INVALID_PARAM;

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return DC_ERROR_UNKNOWN;
    }
    uint64_t now_ms = (uint64_t)ts.tv_sec * 1000ULL + ((uint64_t)ts.tv_nsec / 1000000ULL);
    if (now_ms < DC_DISCORD_EPOCH) return DC_ERROR_INVALID_PARAM;

    uint64_t timestamp_part = (now_ms - DC_DISCORD_EPOCH) << 22;
    uint64_t worker_part = ((uint64_t)(worker_id & 0x1F)) << 17;
    uint64_t process_part = ((uint64_t)(process_id & 0x1F)) << 12;
    uint64_t inc_part = (uint64_t)(increment & 0xFFF);

    *snowflake = (dc_snowflake_t)(timestamp_part | worker_part | process_part | inc_part);
    return DC_OK;
}
