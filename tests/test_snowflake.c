/**
 * @file test_snowflake.c
 * @brief Snowflake tests (placeholder)
 */

#include "test_utils.h"
#include "core/dc_snowflake.h"

int test_snowflake_main(void) {
    TEST_SUITE_BEGIN("Snowflake Tests");

    const char* sample = "175928847299117063";
    dc_snowflake_t snow = DC_SNOWFLAKE_NULL;
    TEST_ASSERT_EQ(DC_OK, dc_snowflake_from_string(sample, &snow), "parse snowflake");

    char buf[32];
    TEST_ASSERT_EQ(DC_OK, dc_snowflake_to_cstr(snow, buf, sizeof(buf)), "snowflake to cstr");
    TEST_ASSERT_STR_EQ(sample, buf, "snowflake roundtrip cstr");

    TEST_ASSERT_EQ(DC_ERROR_PARSE_ERROR, dc_snowflake_from_string("", &snow), "parse empty");
    TEST_ASSERT_EQ(DC_ERROR_PARSE_ERROR, dc_snowflake_from_string("123a", &snow), "parse non-digit");
    TEST_ASSERT_EQ(DC_ERROR_PARSE_ERROR, dc_snowflake_from_string("18446744073709551616", &snow), "parse overflow");

    uint64_t ts_ms = DC_DISCORD_EPOCH + 123456789ULL;
    dc_snowflake_t custom = (dc_snowflake_t)((ts_ms - DC_DISCORD_EPOCH) << 22);
    custom |= ((dc_snowflake_t)5 << 17);
    custom |= ((dc_snowflake_t)7 << 12);
    custom |= (dc_snowflake_t)4095;

    uint64_t out_ts = 0;
    uint8_t worker = 0;
    uint8_t process = 0;
    uint16_t inc = 0;

    TEST_ASSERT_EQ(DC_OK, dc_snowflake_timestamp(custom, &out_ts), "timestamp extract");
    TEST_ASSERT_EQ(ts_ms, out_ts, "timestamp value");
    TEST_ASSERT_EQ(DC_OK, dc_snowflake_worker_id(custom, &worker), "worker extract");
    TEST_ASSERT_EQ(5, worker, "worker value");
    TEST_ASSERT_EQ(DC_OK, dc_snowflake_process_id(custom, &process), "process extract");
    TEST_ASSERT_EQ(7, process, "process value");
    TEST_ASSERT_EQ(DC_OK, dc_snowflake_increment(custom, &inc), "increment extract");
    TEST_ASSERT_EQ(4095, inc, "increment value");

    TEST_SUITE_END("Snowflake Tests");
}
