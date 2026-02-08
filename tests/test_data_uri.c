/**
 * @file test_data_uri.c
 * @brief Data URI tests
 */

#include "test_utils.h"
#include "core/dc_data_uri.h"

int test_data_uri_main(void) {
    TEST_SUITE_BEGIN("Data URI Tests");

    TEST_ASSERT(dc_data_uri_is_valid_image_base64("data:image/png;base64,YWJj"), "valid data uri png");
    TEST_ASSERT(dc_data_uri_is_valid_image_base64("data:image/jpeg;base64,QUJDRA=="), "valid data uri jpeg");
    TEST_ASSERT(!dc_data_uri_is_valid_image_base64("data:text/plain;base64,YWJj"), "invalid data uri type");
    TEST_ASSERT(!dc_data_uri_is_valid_image_base64("data:image/png;base64,YWJ"), "invalid base64 length");
    TEST_ASSERT(!dc_data_uri_is_valid_image_base64("data:image/png;base64,@@@="), "invalid base64 chars");

    dc_string_t out;
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&out), "data uri init");
    TEST_ASSERT_EQ(DC_OK, dc_data_uri_build_image_base64(DC_CDN_IMAGE_PNG, "YWJj", &out),
                   "data uri build png");
    TEST_ASSERT_STR_EQ("data:image/png;base64,YWJj", dc_string_cstr(&out), "data uri png value");
    TEST_ASSERT_EQ(DC_OK, dc_data_uri_build_image_base64(DC_CDN_IMAGE_JPG, "QUJDRA==", &out),
                   "data uri build jpg");
    TEST_ASSERT_STR_EQ("data:image/jpeg;base64,QUJDRA==", dc_string_cstr(&out), "data uri jpg value");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_data_uri_build_image_base64(DC_CDN_IMAGE_PNG, "bad@@", &out),
                   "data uri invalid base64");
    dc_string_free(&out);

    TEST_SUITE_END("Data URI Tests");
}
