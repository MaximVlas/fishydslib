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

BENCHMARK_MAIN();
