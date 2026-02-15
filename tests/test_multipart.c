/**
 * @file test_multipart.c
 * @brief Multipart tests
 */

#include "test_utils.h"
#include "http/dc_multipart.h"
#include "core/dc_string.h"
#include <string.h>

int test_multipart_main(void) {
    TEST_SUITE_BEGIN("Multipart Tests");

    dc_multipart_t mp;
    TEST_ASSERT_EQ(DC_OK, dc_multipart_init(&mp), "multipart init");
    TEST_ASSERT_EQ(DC_OK, dc_multipart_set_boundary(&mp, "safe-BOUNDARY_01"),
                   "multipart custom boundary");

    dc_string_t content_type;
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&content_type), "multipart content type init");
    TEST_ASSERT_EQ(DC_OK, dc_multipart_get_content_type(&mp, &content_type), "multipart content type");
    TEST_ASSERT(strstr(dc_string_cstr(&content_type), "multipart/form-data; boundary=") != NULL,
                "multipart content type value");
    dc_string_free(&content_type);

    const char* json = "{\"content\":\"hi\"}";
    TEST_ASSERT_EQ(DC_OK, dc_multipart_add_payload_json(&mp, json), "multipart payload_json");

    const char data[] = "DATA";
    size_t index = 0;
    TEST_ASSERT_EQ(DC_OK,
                   dc_multipart_add_file(&mp, "file.png", data, sizeof(data) - 1,
                                         "image/png", &index),
                   "multipart add file");
    TEST_ASSERT_EQ(0u, index, "multipart file index");

    TEST_ASSERT_EQ(DC_OK, dc_multipart_finish(&mp), "multipart finish");

    const char* body = dc_string_cstr(&mp.body);
    TEST_ASSERT(body && body[0] != '\0', "multipart body not empty");
    TEST_ASSERT(strstr(body, "name=\"payload_json\"") != NULL, "multipart body payload_json header");
    TEST_ASSERT(strstr(body, "files[0]") != NULL, "multipart body file field");
    TEST_ASSERT(strstr(body, "filename=\"file.png\"") != NULL, "multipart body filename");

    dc_multipart_free(&mp);

    dc_multipart_t limit;
    TEST_ASSERT_EQ(DC_OK, dc_multipart_init(&limit), "multipart limit init");
    TEST_ASSERT_EQ(DC_OK, dc_multipart_set_limits(&limit, (size_t)3u, (size_t)0u), "multipart limit set");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_multipart_add_file(&limit, "file.png", data, sizeof(data) - 1,
                                         "image/png", NULL),
                   "multipart limit reject");
    dc_multipart_free(&limit);

    dc_multipart_t bad_char;
    TEST_ASSERT_EQ(DC_OK, dc_multipart_init(&bad_char), "multipart bad char init");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_multipart_set_boundary(&bad_char, "bad boundary"),
                   "multipart boundary rejects spaces");
    dc_multipart_free(&bad_char);

    dc_multipart_t too_long;
    TEST_ASSERT_EQ(DC_OK, dc_multipart_init(&too_long), "multipart long boundary init");
    char long_boundary[72];
    memset(long_boundary, 'a', sizeof(long_boundary) - 1u);
    long_boundary[sizeof(long_boundary) - 1u] = '\0';
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_multipart_set_boundary(&too_long, long_boundary),
                   "multipart boundary max length 70");
    dc_multipart_free(&too_long);

    TEST_SUITE_END("Multipart Tests");
}
