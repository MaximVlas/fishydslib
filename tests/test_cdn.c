/**
 * @file test_cdn.c
 * @brief CDN helper tests
 */

#include "test_utils.h"
#include "core/dc_cdn.h"
#include "core/dc_string.h"

int test_cdn_main(void) {
    TEST_SUITE_BEGIN("CDN Tests");

    TEST_ASSERT(dc_cdn_image_format_is_valid(DC_CDN_IMAGE_PNG), "format png valid");
    TEST_ASSERT(!dc_cdn_image_format_is_valid((dc_cdn_image_format_t)99), "format invalid");

    TEST_ASSERT(dc_cdn_image_extension_is_valid("png"), "ext png valid");
    TEST_ASSERT(dc_cdn_image_extension_is_valid(".jpg"), "ext jpg valid");
    TEST_ASSERT(dc_cdn_image_extension_is_valid("jpeg"), "ext jpeg valid");
    TEST_ASSERT(dc_cdn_image_extension_is_valid("webp"), "ext webp valid");
    TEST_ASSERT(dc_cdn_image_extension_is_valid("avif"), "ext avif valid");
    TEST_ASSERT(!dc_cdn_image_extension_is_valid("bmp"), "ext bmp invalid");

    TEST_ASSERT(dc_cdn_image_size_is_valid(16), "size 16 valid");
    TEST_ASSERT(dc_cdn_image_size_is_valid(256), "size 256 valid");
    TEST_ASSERT(dc_cdn_image_size_is_valid(4096), "size 4096 valid");
    TEST_ASSERT(!dc_cdn_image_size_is_valid(15), "size 15 invalid");
    TEST_ASSERT(!dc_cdn_image_size_is_valid(24), "size 24 invalid");
    TEST_ASSERT(!dc_cdn_image_size_is_valid(8192), "size 8192 invalid");

    TEST_ASSERT(dc_cdn_hash_is_animated("a_123"), "hash animated true");
    TEST_ASSERT(!dc_cdn_hash_is_animated("b_123"), "hash animated false");
    TEST_ASSERT(!dc_cdn_hash_is_animated(NULL), "hash animated null false");

    dc_string_t url;
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&url), "cdn url init");
    TEST_ASSERT_EQ(DC_OK,
                   dc_cdn_build_url(NULL, "/avatars/123/hash",
                                    DC_CDN_FMT_ALL, DC_CDN_IMAGE_PNG, 128,
                                    0, 0, &url),
                   "cdn build url");
    TEST_ASSERT_STR_EQ("https://cdn.discordapp.com/avatars/123/hash.png?size=128",
                       dc_string_cstr(&url), "cdn url value");

    TEST_ASSERT_EQ(DC_OK,
                   dc_cdn_build_url("https://cdn.discordapp.com/", "/icons/1/icon",
                                    DC_CDN_FMT_ALL, DC_CDN_IMAGE_WEBP, 0,
                                    0, 0, &url),
                   "cdn build url base slash");
    TEST_ASSERT_STR_EQ("https://cdn.discordapp.com/icons/1/icon.webp",
                       dc_string_cstr(&url), "cdn url base slash value");

    TEST_ASSERT_EQ(DC_OK,
                   dc_cdn_user_avatar(123, "a_hash", DC_CDN_IMAGE_PNG, 64, 1, &url),
                   "cdn user avatar animated");
    TEST_ASSERT_STR_EQ("https://cdn.discordapp.com/avatars/123/a_hash.gif?size=64",
                       dc_string_cstr(&url), "cdn user avatar animated value");

    TEST_ASSERT_EQ(DC_OK,
                   dc_cdn_emoji(42, 1, DC_CDN_IMAGE_PNG, 64, &url),
                   "cdn emoji animated");
    TEST_ASSERT_STR_EQ("https://cdn.discordapp.com/emojis/42.gif?size=64",
                       dc_string_cstr(&url), "cdn emoji animated value");

    TEST_ASSERT_EQ(DC_OK, dc_cdn_attachment_url_passthrough("https://cdn.discordapp.com/attachments/a", &url),
                   "cdn attachment passthrough");
    TEST_ASSERT_STR_EQ("https://cdn.discordapp.com/attachments/a", dc_string_cstr(&url),
                       "cdn attachment passthrough value");

    dc_string_free(&url);

    TEST_SUITE_END("CDN Tests");
}
