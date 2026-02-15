/**
 * @file bench_multipart.cc
 * @brief Multipart benchmarks
 */

#include <benchmark/benchmark.h>
#include <string.h>

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

static void BM_Multipart_PayloadOnly(benchmark::State& state) {
    const char* json = "{\"content\":\"hello world\",\"tts\":false}";
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_multipart_t mp;
        dc_multipart_init(&mp);
        dc_multipart_add_payload_json(&mp, json);
        dc_multipart_finish(&mp);
        total_bytes += dc_string_length(&mp.body);
        benchmark::DoNotOptimize(dc_string_length(&mp.body));
        dc_multipart_free(&mp);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Multipart_PayloadOnly);

static void BM_Multipart_MultipleFiles(benchmark::State& state) {
    const char* json = "{\"content\":\"multiple files\"}";
    const char file1[] = "PNG_DATA_PLACEHOLDER";
    const char file2[] = "JPEG_DATA_PLACEHOLDER_LONGER";
    const char file3[] = "GIF_PLACEHOLDER";
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_multipart_t mp;
        dc_multipart_init(&mp);
        dc_multipart_add_payload_json(&mp, json);
        dc_multipart_add_file(&mp, "image.png", file1, sizeof(file1) - 1, "image/png", NULL);
        dc_multipart_add_file(&mp, "photo.jpg", file2, sizeof(file2) - 1, "image/jpeg", NULL);
        dc_multipart_add_file(&mp, "anim.gif", file3, sizeof(file3) - 1, "image/gif", NULL);
        dc_multipart_finish(&mp);
        total_bytes += dc_string_length(&mp.body);
        benchmark::DoNotOptimize(dc_string_length(&mp.body));
        dc_multipart_free(&mp);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Multipart_MultipleFiles);

static void BM_Multipart_LargeFile(benchmark::State& state) {
    const size_t size = static_cast<size_t>(state.range(0));
    char* data = new char[size];
    memset(data, 'X', size);
    const char* json = "{\"content\":\"large\"}";
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_multipart_t mp;
        dc_multipart_init(&mp);
        dc_multipart_add_payload_json(&mp, json);
        dc_multipart_add_file(&mp, "big.bin", data, size, "application/octet-stream", NULL);
        dc_multipart_finish(&mp);
        total_bytes += dc_string_length(&mp.body);
        benchmark::DoNotOptimize(dc_string_length(&mp.body));
        dc_multipart_free(&mp);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    delete[] data;
}
BENCHMARK(BM_Multipart_LargeFile)->Range(1024, 1 << 20);

static void BM_Multipart_Fields(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_multipart_t mp;
        dc_multipart_init(&mp);
        dc_multipart_add_field(&mp, "username", "testbot");
        dc_multipart_add_field(&mp, "avatar_url", "https://cdn.discordapp.com/avatars/123/abc.png");
        dc_multipart_add_field(&mp, "content", "message content here");
        dc_multipart_finish(&mp);
        total_bytes += dc_string_length(&mp.body);
        benchmark::DoNotOptimize(dc_string_length(&mp.body));
        dc_multipart_free(&mp);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Multipart_Fields);

static void BM_Multipart_CustomBoundary(benchmark::State& state) {
    const char* json = "{\"content\":\"custom\"}";
    const char payload[] = "DATA";
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_multipart_t mp;
        dc_multipart_init(&mp);
        dc_multipart_set_boundary(&mp, "----WebKitFormBoundary7MA4YWxkTrZu0gW");
        dc_multipart_add_payload_json(&mp, json);
        dc_multipart_add_file(&mp, "file.png", payload, sizeof(payload) - 1, "image/png", NULL);
        dc_multipart_finish(&mp);
        total_bytes += dc_string_length(&mp.body);
        benchmark::DoNotOptimize(dc_string_length(&mp.body));
        dc_multipart_free(&mp);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Multipart_CustomBoundary);

static void BM_Multipart_ContentType(benchmark::State& state) {
    dc_multipart_t mp;
    dc_multipart_init(&mp);
    dc_multipart_set_boundary(&mp, "boundary123");
    dc_string_t out;
    dc_string_init(&out);
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_multipart_get_content_type(&mp, &out);
        total_bytes += dc_string_length(&out);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    dc_string_free(&out);
    dc_multipart_free(&mp);
}
BENCHMARK(BM_Multipart_ContentType);

static void BM_Multipart_FileNamed(benchmark::State& state) {
    const char* json = "{\"content\":\"named\"}";
    const char payload[] = "DATA";
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_multipart_t mp;
        dc_multipart_init(&mp);
        dc_multipart_add_payload_json(&mp, json);
        dc_multipart_add_file_named(&mp, "files[0]", "image.png",
                                    payload, sizeof(payload) - 1, "image/png");
        dc_multipart_finish(&mp);
        total_bytes += dc_string_length(&mp.body);
        benchmark::DoNotOptimize(dc_string_length(&mp.body));
        dc_multipart_free(&mp);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Multipart_FileNamed);

BENCHMARK_MAIN();
