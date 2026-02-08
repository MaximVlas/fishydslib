/**
 * @file bench_core.cc
 * @brief Core benchmarks with throughput metrics
 */

#include <benchmark/benchmark.h>
#include <cstring>

extern "C" {
#include "core/dc_alloc.h"
#include "core/dc_string.h"
#include "core/dc_vec.h"
#include "core/dc_snowflake.h"
#include "core/dc_time.h"
}

static char kBenchBuffer[4096];
static uint64_t kBenchValues[4096];

static void dc_bench_init_buffers() {
    static int initialized = 0;
    if (initialized) return;
    for (size_t i = 0; i < sizeof(kBenchBuffer); i++) {
        kBenchBuffer[i] = (char)('a' + (i % 26));
    }
    for (size_t i = 0; i < sizeof(kBenchValues) / sizeof(kBenchValues[0]); i++) {
        kBenchValues[i] = (uint64_t)(i + 1);
    }
    initialized = 1;
}

static void BM_Alloc_Free(benchmark::State& state) {
    const size_t size = static_cast<size_t>(state.range(0));
    size_t total_bytes = 0;
    for (auto _ : state) {
        void* ptr = dc_alloc(size);
        benchmark::DoNotOptimize(ptr);
        dc_free(ptr);
        total_bytes += size;
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Alloc_Free)->Range(8, 4096);

static void BM_Calloc_Free(benchmark::State& state) {
    const size_t count = static_cast<size_t>(state.range(0));
    size_t total_bytes = 0;
    for (auto _ : state) {
        void* ptr = dc_calloc(count, sizeof(uint64_t));
        benchmark::DoNotOptimize(ptr);
        dc_free(ptr);
        total_bytes += count * sizeof(uint64_t);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Calloc_Free)->Range(1, 2048);

static void BM_Realloc_Grow(benchmark::State& state) {
    const size_t start = static_cast<size_t>(state.range(0));
    const size_t end = start * 8;
    size_t total_bytes = 0;
    for (auto _ : state) {
        void* ptr = dc_alloc(start);
        benchmark::DoNotOptimize(ptr);
        ptr = dc_realloc(ptr, end);
        benchmark::DoNotOptimize(ptr);
        dc_free(ptr);
        total_bytes += end;
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Realloc_Grow)->Range(16, 1024);

static void BM_Snowflake_Parse(benchmark::State& state) {
    const char* sample = "175928847299117063";
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_snowflake_t snow = 0;
        dc_status_t st = dc_snowflake_from_string(sample, &snow);
        benchmark::DoNotOptimize(st);
        benchmark::DoNotOptimize(snow);
        total_bytes += strlen(sample);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Snowflake_Parse);

static void BM_Time_Parse(benchmark::State& state) {
    const char* iso = "2023-01-01T12:34:56.789Z";
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_iso8601_t ts;
        dc_status_t st = dc_iso8601_parse(iso, &ts);
        benchmark::DoNotOptimize(st);
        benchmark::DoNotOptimize(ts.year);
        total_bytes += strlen(iso);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Time_Parse);

static void BM_String_Append(benchmark::State& state) {
    const char* part = "abcdefghij";
    const size_t reps = static_cast<size_t>(state.range(0));
    const size_t part_len = strlen(part);
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_t s;
        dc_string_init(&s);
        for (size_t i = 0; i < reps; i++) {
            dc_string_append_cstr(&s, part);
        }
        benchmark::DoNotOptimize(dc_string_length(&s));
        dc_string_free(&s);
        total_bytes += reps * part_len;
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * reps));
}
BENCHMARK(BM_String_Append)->Range(8, 1024);

static void BM_String_Set_Buffer(benchmark::State& state) {
    dc_bench_init_buffers();
    const size_t len = static_cast<size_t>(state.range(0));
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_t s;
        dc_string_init(&s);
        dc_string_set_buffer(&s, kBenchBuffer, len);
        benchmark::DoNotOptimize(dc_string_length(&s));
        dc_string_free(&s);
        total_bytes += len;
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_String_Set_Buffer)->Range(8, 2048);

static void BM_String_Append_Buffer(benchmark::State& state) {
    dc_bench_init_buffers();
    const size_t len = static_cast<size_t>(state.range(0));
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_t s;
        dc_string_init(&s);
        dc_string_append_buffer(&s, kBenchBuffer, len);
        benchmark::DoNotOptimize(dc_string_length(&s));
        dc_string_free(&s);
        total_bytes += len;
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_String_Append_Buffer)->Range(8, 2048);

static void BM_Vec_Push(benchmark::State& state) {
    const size_t count = static_cast<size_t>(state.range(0));
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_vec_t vec;
        dc_vec_init(&vec, sizeof(uint64_t));
        for (size_t i = 0; i < count; i++) {
            uint64_t v = static_cast<uint64_t>(i);
            dc_vec_push(&vec, &v);
        }
        benchmark::DoNotOptimize(dc_vec_length(&vec));
        dc_vec_free(&vec);
        total_bytes += count * sizeof(uint64_t);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * count));
}
BENCHMARK(BM_Vec_Push)->Range(16, 4096);

static void BM_Vec_Append(benchmark::State& state) {
    dc_bench_init_buffers();
    const size_t count = static_cast<size_t>(state.range(0));
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_vec_t vec;
        dc_vec_init(&vec, sizeof(uint64_t));
        dc_vec_append(&vec, kBenchValues, count);
        benchmark::DoNotOptimize(dc_vec_length(&vec));
        dc_vec_free(&vec);
        total_bytes += count * sizeof(uint64_t);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * count));
}
BENCHMARK(BM_Vec_Append)->Range(16, 4096);

static void BM_Snowflake_To_Cstr(benchmark::State& state) {
    const dc_snowflake_t snow = 175928847299117063ULL;
    char buf[32];
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_status_t st = dc_snowflake_to_cstr(snow, buf, sizeof(buf));
        benchmark::DoNotOptimize(st);
        benchmark::DoNotOptimize(buf[0]);
        total_bytes += strlen(buf);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Snowflake_To_Cstr);

static void BM_Time_Format(benchmark::State& state) {
    dc_iso8601_t ts = {
        2024, 1, 1, 12, 34, 56, 789, 0, 1
    };
    char buf[64];
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_status_t st = dc_iso8601_format_cstr(&ts, buf, sizeof(buf));
        benchmark::DoNotOptimize(st);
        benchmark::DoNotOptimize(buf[0]);
        total_bytes += strlen(buf);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Time_Format);

BENCHMARK_MAIN();
