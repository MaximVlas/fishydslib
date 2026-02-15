/**
 * @file bench_cdn.cc
 * @brief CDN benchmarks
 */

#include <benchmark/benchmark.h>

extern "C" {
#include "core/dc_cdn.h"
#include "core/dc_string.h"
}

static void BM_CDN_UserAvatar(benchmark::State& state) {
    dc_string_t out;
    dc_string_init(&out);
    for (auto _ : state) {
        dc_cdn_user_avatar(123456789012345678ULL, "a_hash", DC_CDN_IMAGE_PNG, 128, 1, &out);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }
    dc_string_free(&out);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_CDN_UserAvatar);

static void BM_CDN_Emoji(benchmark::State& state) {
    dc_string_t out;
    dc_string_init(&out);
    for (auto _ : state) {
        dc_cdn_emoji(987654321, 0, DC_CDN_IMAGE_WEBP, 64, &out);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }
    dc_string_free(&out);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_CDN_Emoji);

static void BM_CDN_GuildIcon(benchmark::State& state) {
    dc_string_t out;
    dc_string_init(&out);
    for (auto _ : state) {
        dc_cdn_guild_icon(111222333444555666ULL, "a_iconhash", DC_CDN_IMAGE_PNG, 256, 1, &out);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }
    dc_string_free(&out);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_CDN_GuildIcon);

static void BM_CDN_ChannelIcon(benchmark::State& state) {
    dc_string_t out;
    dc_string_init(&out);
    for (auto _ : state) {
        dc_cdn_channel_icon(999888777666555444ULL, "channelhash", DC_CDN_IMAGE_WEBP, 128, 0, &out);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }
    dc_string_free(&out);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_CDN_ChannelIcon);

static void BM_CDN_HashIsAnimated(benchmark::State& state) {
    const char* hashes[] = {
        "a_abcdef1234567890",
        "abcdef1234567890",
        "a_",
        "",
        "a_longhashvalue12345"
    };
    size_t count = sizeof(hashes) / sizeof(hashes[0]);
    for (auto _ : state) {
        for (size_t i = 0; i < count; i++) {
            int ok = dc_cdn_hash_is_animated(hashes[i]);
            benchmark::DoNotOptimize(ok);
        }
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(count));
}
BENCHMARK(BM_CDN_HashIsAnimated);

static void BM_CDN_ImageSizeValid(benchmark::State& state) {
    const uint32_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 100, 0, 8192};
    size_t count = sizeof(sizes) / sizeof(sizes[0]);
    for (auto _ : state) {
        for (size_t i = 0; i < count; i++) {
            int ok = dc_cdn_image_size_is_valid(sizes[i]);
            benchmark::DoNotOptimize(ok);
        }
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(count));
}
BENCHMARK(BM_CDN_ImageSizeValid);

static void BM_CDN_ImageFormatValid(benchmark::State& state) {
    dc_cdn_image_format_t formats[] = {
        DC_CDN_IMAGE_PNG, DC_CDN_IMAGE_JPG, DC_CDN_IMAGE_WEBP,
        DC_CDN_IMAGE_GIF, DC_CDN_IMAGE_AVIF,
        (dc_cdn_image_format_t)99
    };
    size_t count = sizeof(formats) / sizeof(formats[0]);
    for (auto _ : state) {
        for (size_t i = 0; i < count; i++) {
            int ok = dc_cdn_image_format_is_valid(formats[i]);
            benchmark::DoNotOptimize(ok);
        }
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(count));
}
BENCHMARK(BM_CDN_ImageFormatValid);

static void BM_CDN_ImageFormatExtension(benchmark::State& state) {
    dc_cdn_image_format_t formats[] = {
        DC_CDN_IMAGE_PNG, DC_CDN_IMAGE_JPG, DC_CDN_IMAGE_WEBP,
        DC_CDN_IMAGE_GIF, DC_CDN_IMAGE_AVIF
    };
    size_t count = sizeof(formats) / sizeof(formats[0]);
    for (auto _ : state) {
        for (size_t i = 0; i < count; i++) {
            const char* ext = dc_cdn_image_format_extension(formats[i]);
            benchmark::DoNotOptimize(ext);
        }
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(count));
}
BENCHMARK(BM_CDN_ImageFormatExtension);

static void BM_CDN_ImageExtensionValid(benchmark::State& state) {
    const char* exts[] = {"png", "jpg", "jpeg", "webp", "gif", "json", "bmp", "tiff"};
    size_t count = sizeof(exts) / sizeof(exts[0]);
    for (auto _ : state) {
        for (size_t i = 0; i < count; i++) {
            int ok = dc_cdn_image_extension_is_valid(exts[i]);
            benchmark::DoNotOptimize(ok);
        }
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(count));
}
BENCHMARK(BM_CDN_ImageExtensionValid);

BENCHMARK_MAIN();
