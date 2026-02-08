/**
 * @file test_time.c
 * @brief Time tests (placeholder)
 */

#include "test_utils.h"
#include "core/dc_time.h"

int test_time_main(void) {
    TEST_SUITE_BEGIN("Time Tests");

    dc_iso8601_t ts;
    TEST_ASSERT_EQ(DC_OK, dc_iso8601_parse("2023-01-01T12:34:56.789Z", &ts), "parse utc");
    TEST_ASSERT_EQ(2023, ts.year, "year");
    TEST_ASSERT_EQ(1, ts.month, "month");
    TEST_ASSERT_EQ(1, ts.day, "day");
    TEST_ASSERT_EQ(12, ts.hour, "hour");
    TEST_ASSERT_EQ(34, ts.minute, "minute");
    TEST_ASSERT_EQ(56, ts.second, "second");
    TEST_ASSERT_EQ(789, ts.millisecond, "millisecond");
    TEST_ASSERT_EQ(1, ts.is_utc, "is utc");
    TEST_ASSERT_EQ(0, ts.utc_offset_minutes, "offset");

    char buf[64];
    TEST_ASSERT_EQ(DC_OK, dc_iso8601_format_cstr(&ts, buf, sizeof(buf)), "format utc");
    TEST_ASSERT_STR_EQ("2023-01-01T12:34:56.789Z", buf, "roundtrip utc");

    uint64_t ms = 0;
    TEST_ASSERT_EQ(DC_OK, dc_iso8601_to_unix_ms(&ts, &ms), "to unix ms");
    TEST_ASSERT_EQ((uint64_t)1672576496789ULL, ms, "unix ms value");

    dc_iso8601_t ts_from;
    TEST_ASSERT_EQ(DC_OK, dc_iso8601_from_unix_ms(1672576496789ULL, &ts_from), "from unix ms");
    TEST_ASSERT_EQ(DC_OK, dc_iso8601_format_cstr(&ts_from, buf, sizeof(buf)), "format from unix");
    TEST_ASSERT_STR_EQ("2023-01-01T12:34:56.789Z", buf, "format from unix value");

    TEST_ASSERT_EQ(DC_OK, dc_iso8601_parse("2023-01-01T12:34:56+02:30", &ts), "parse offset");
    TEST_ASSERT_EQ(150, ts.utc_offset_minutes, "offset minutes");
    TEST_ASSERT_EQ(0, ts.is_utc, "is not utc");
    TEST_ASSERT_EQ(DC_OK, dc_iso8601_format_cstr(&ts, buf, sizeof(buf)), "format offset");
    TEST_ASSERT_STR_EQ("2023-01-01T12:34:56+02:30", buf, "roundtrip offset");
    TEST_ASSERT_EQ(DC_OK, dc_iso8601_to_unix_ms(&ts, &ms), "offset to unix ms");
    TEST_ASSERT_EQ((uint64_t)1672567496000ULL, ms, "offset unix ms value");

    dc_iso8601_t invalid = { .year = 2023, .month = 13, .day = 1, .hour = 0, .minute = 0, .second = 0, .millisecond = 0, .utc_offset_minutes = 0, .is_utc = 1 };
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_iso8601_validate(&invalid), "invalid month");

    TEST_SUITE_END("Time Tests");
}
