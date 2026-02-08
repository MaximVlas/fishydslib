/**
 * @file bench_multipart.cc
 * @brief Multipart benchmarks
 */

#include <benchmark/benchmark.h>

extern "C" {
#include "http/dc_multipart.h"
#include "core/dc_string.h"
}

static void BM_Multipart_Build(benchmark::State& state) {
    const char* json = "{\"content\":\"hello\"}";
    const char payload[] = "DATA";
    for (auto _ : state) {
        dc_multipart_t mp;
        dc_multipart_init(&mp);
        dc_multipart_add_payload_json(&mp, json);
        dc_multipart_add_file(&mp, "file.png", payload, sizeof(payload) - 1, "image/png", NULL);
        dc_multipart_finish(&mp);
        benchmark::DoNotOptimize(dc_string_length(&mp.body));
        dc_multipart_free(&mp);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Multipart_Build);

BENCHMARK_MAIN();
