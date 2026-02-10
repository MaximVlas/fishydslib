/**
 * @file test_attachments.c
 * @brief Attachment helper tests
 */

#include "test_utils.h"
#include "core/dc_attachments.h"

int test_attachments_main(void) {
    TEST_SUITE_BEGIN("Attachments Tests");

    TEST_ASSERT(!dc_attachment_filename_is_valid(NULL), "filename NULL invalid");
    TEST_ASSERT(!dc_attachment_filename_is_valid(""), "filename empty invalid");
    TEST_ASSERT(dc_attachment_filename_is_valid("file.png"), "filename valid png");
    TEST_ASSERT(dc_attachment_filename_is_valid("my_file-1.png"), "filename valid underscore dash");
    TEST_ASSERT(dc_attachment_filename_is_valid("A1.B2"), "filename valid dots");
    TEST_ASSERT(!dc_attachment_filename_is_valid("file name.png"), "filename invalid space");
    TEST_ASSERT(!dc_attachment_filename_is_valid("../file.png"), "filename invalid path");
    TEST_ASSERT(!dc_attachment_filename_is_valid("file@.png"), "filename invalid char");
    TEST_ASSERT(!dc_attachment_filename_is_valid("file~.png"), "filename invalid tilde");
    TEST_ASSERT(!dc_attachment_filename_is_valid("file\x7f.png"), "filename invalid DEL");
    TEST_ASSERT(!dc_attachment_filename_is_valid("file\xC3\xB1.png"), "filename invalid non-ascii");

    TEST_ASSERT(dc_attachment_size_is_valid(10, 100), "size valid");
    TEST_ASSERT(dc_attachment_size_is_valid(0, 100), "size zero valid");
    TEST_ASSERT(!dc_attachment_size_is_valid(200, 100), "size too large invalid");
    TEST_ASSERT(dc_attachment_size_is_valid(200, 0), "size unlimited valid");

    TEST_ASSERT(dc_attachment_total_size_is_valid(100, 1000), "total size valid");
    TEST_ASSERT(!dc_attachment_total_size_is_valid(2000, 1000), "total size too large");
    TEST_ASSERT(dc_attachment_total_size_is_valid(2000, 0), "total size unlimited");

    TEST_SUITE_END("Attachments Tests");
}
