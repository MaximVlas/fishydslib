/**
 * @file test_status.c
 * @brief Status code tests
 */

#include "test_utils.h"
#include "core/dc_status.h"

void test_status_string(void) {
    TEST_ASSERT_STR_EQ("Success", dc_status_string(DC_OK), "DC_OK string");
    TEST_ASSERT_STR_EQ("Invalid parameter", dc_status_string(DC_ERROR_INVALID_PARAM), "DC_ERROR_INVALID_PARAM string");
    TEST_ASSERT_STR_EQ("Out of memory", dc_status_string(DC_ERROR_OUT_OF_MEMORY), "DC_ERROR_OUT_OF_MEMORY string");
    TEST_ASSERT_STR_EQ("Not implemented", dc_status_string(DC_ERROR_NOT_IMPLEMENTED), "DC_ERROR_NOT_IMPLEMENTED string");
    TEST_ASSERT_STR_EQ("Bad request", dc_status_string(DC_ERROR_BAD_REQUEST), "DC_ERROR_BAD_REQUEST string");
}

void test_status_recoverable(void) {
    TEST_ASSERT_EQ(0, dc_status_is_recoverable(DC_OK), "DC_OK not recoverable");
    TEST_ASSERT_EQ(0, dc_status_is_recoverable(DC_ERROR_INVALID_PARAM), "DC_ERROR_INVALID_PARAM not recoverable");
    TEST_ASSERT_EQ(1, dc_status_is_recoverable(DC_ERROR_NETWORK), "DC_ERROR_NETWORK recoverable");
    TEST_ASSERT_EQ(1, dc_status_is_recoverable(DC_ERROR_TIMEOUT), "DC_ERROR_TIMEOUT recoverable");
    TEST_ASSERT_EQ(1, dc_status_is_recoverable(DC_ERROR_RATE_LIMITED), "DC_ERROR_RATE_LIMITED recoverable");
    TEST_ASSERT_EQ(1, dc_status_is_recoverable(DC_ERROR_SERVER), "DC_ERROR_SERVER recoverable");
}

void test_status_from_http(void) {
    TEST_ASSERT_EQ(DC_OK, dc_status_from_http(200), "HTTP 200 OK");
    TEST_ASSERT_EQ(DC_ERROR_NOT_MODIFIED, dc_status_from_http(304), "HTTP 304 not modified");
    TEST_ASSERT_EQ(DC_ERROR_BAD_REQUEST, dc_status_from_http(400), "HTTP 400 bad request");
    TEST_ASSERT_EQ(DC_ERROR_UNAUTHORIZED, dc_status_from_http(401), "HTTP 401 unauthorized");
    TEST_ASSERT_EQ(DC_ERROR_FORBIDDEN, dc_status_from_http(403), "HTTP 403 forbidden");
    TEST_ASSERT_EQ(DC_ERROR_NOT_FOUND, dc_status_from_http(404), "HTTP 404 not found");
    TEST_ASSERT_EQ(DC_ERROR_METHOD_NOT_ALLOWED, dc_status_from_http(405), "HTTP 405 method not allowed");
    TEST_ASSERT_EQ(DC_ERROR_RATE_LIMITED, dc_status_from_http(429), "HTTP 429 rate limited");
    TEST_ASSERT_EQ(DC_ERROR_UNAVAILABLE, dc_status_from_http(502), "HTTP 502 unavailable");
    TEST_ASSERT_EQ(DC_ERROR_SERVER, dc_status_from_http(500), "HTTP 500 server error");
}

void test_result_macros(void) {
    DC_RESULT(int) result = DC_OK_RESULT(42);
    TEST_ASSERT(DC_IS_OK(result), "DC_IS_OK works");
    TEST_ASSERT_EQ(42, DC_VALUE(result), "DC_VALUE works");
    TEST_ASSERT_EQ(DC_OK, DC_STATUS(result), "DC_STATUS works");

    typedef struct {
        int dummy;
    } test_val_t;

    DC_RESULT(test_val_t) error_result = DC_ERROR_RESULT(DC_ERROR_INVALID_PARAM);
    TEST_ASSERT(DC_IS_ERROR(error_result), "DC_IS_ERROR works");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, DC_STATUS(error_result), "Error status works");
}

int test_status_main(void) {
    TEST_SUITE_BEGIN("Status Tests");
    
    test_status_string();
    test_status_recoverable();
    test_status_from_http();
    test_result_macros();
    
    TEST_SUITE_END("Status Tests");
}
