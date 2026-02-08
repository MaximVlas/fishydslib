/**
 * @file bench_format.cc
 * @brief Format benchmarks
 */

#include <benchmark/benchmark.h>

extern "C" {
#include "core/dc_format.h"
#include "core/dc_allowed_mentions.h"
#include "json/dc_json.h"
}

static void BM_Format_Mention_User(benchmark::State& state) {
    dc_string_t out;
    dc_string_init(&out);
    for (auto _ : state) {
        dc_format_mention_user(123456789012345678ULL, &out);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }
    dc_string_free(&out);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Format_Mention_User);

static void BM_Format_Timestamp(benchmark::State& state) {
    dc_string_t out;
    dc_string_init(&out);
    for (auto _ : state) {
        dc_format_timestamp(1700000000, 'R', &out);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }
    dc_string_free(&out);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Format_Timestamp);

static void BM_Format_Escape(benchmark::State& state) {
    const char* text = "Hello @everyone <#123> **bold**";
    dc_string_t out;
    dc_string_init(&out);
    for (auto _ : state) {
        dc_format_escape_content(text, &out);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }
    dc_string_free(&out);
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * 32));
}
BENCHMARK(BM_Format_Escape);

static void BM_AllowedMentions_Build(benchmark::State& state) {
    dc_allowed_mentions_t mentions;
    dc_allowed_mentions_init(&mentions);
    dc_allowed_mentions_set_parse(&mentions, 1, 1, 0);
    dc_allowed_mentions_set_replied_user(&mentions, 1);
    dc_allowed_mentions_add_user(&mentions, 123);
    dc_allowed_mentions_add_role(&mentions, 456);

    dc_string_t out;
    dc_string_init(&out);

    for (auto _ : state) {
        dc_json_mut_doc_t doc;
        dc_json_mut_doc_create(&doc);
        dc_json_mut_add_allowed_mentions(&doc, doc.root, "allowed_mentions", &mentions);
        dc_json_mut_doc_serialize(&doc, &out);
        dc_json_mut_doc_free(&doc);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }

    dc_string_free(&out);
    dc_allowed_mentions_free(&mentions);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_AllowedMentions_Build);

BENCHMARK_MAIN();
