/**
 * @file bench_http.cc
 * @brief HTTP benchmarks with throughput metrics
 */

#include <benchmark/benchmark.h>
#include <stdio.h>
#include <string.h>

extern "C" {
#include "core/dc_string.h"
#include "http/dc_http.h"
#include "http/dc_http_compliance.h"
}

typedef struct {
    const char* name;
    const char* value;
} dc_bench_header_kv_t;

static const dc_bench_header_kv_t kRateLimitHeaders[] = {
    {"X-RateLimit-Limit", "5"},
    {"X-RateLimit-Remaining", "4"},
    {"X-RateLimit-Reset", "1699999999"},
    {"X-RateLimit-Reset-After", "1.234"},
    {"X-RateLimit-Bucket", "bucket123"},
    {"X-RateLimit-Global", "false"},
    {"X-RateLimit-Scope", "user"},
    {"Retry-After", "0.5"},
    {NULL, NULL}
};

static dc_status_t dc_bench_get_header(void* userdata, const char* name, const char** value) {
    const dc_bench_header_kv_t* headers = (const dc_bench_header_kv_t*)userdata;
    if (!headers || !name || !value) return DC_ERROR_NULL_POINTER;
    for (size_t i = 0; headers[i].name != NULL; i++) {
        if (strcmp(headers[i].name, name) == 0) {
            *value = headers[i].value;
            return DC_OK;
        }
    }
    *value = NULL;
    return DC_ERROR_NOT_FOUND;
}

static const char* kRateLimitBody =
    "{\"message\":\"You are being rate limited.\",\"retry_after\":1.234,"
    "\"global\":false,\"code\":0}";

static const char* kUserAgentValue = "DiscordBot (https://example.com, 1.0.0) fishydslib";
static const char* kContentTypeValue = "application/json; charset=utf-8";

static void BM_HTTP_Build_URL(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_t url;
        dc_string_init(&url);
        dc_http_build_discord_api_url("/users/@me", &url);
        total_bytes += dc_string_length(&url);
        benchmark::DoNotOptimize(dc_string_length(&url));
        dc_string_free(&url);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_HTTP_Build_URL);

static void BM_HTTP_Format_UserAgent(benchmark::State& state) {
    dc_user_agent_t ua = {
        "fishydslib",
        "0.1.0",
        "https://example.com"
    };
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_t out;
        dc_string_init(&out);
        dc_http_format_user_agent(&ua, &out);
        total_bytes += dc_string_length(&out);
        benchmark::DoNotOptimize(dc_string_length(&out));
        dc_string_free(&out);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_HTTP_Format_UserAgent);

static void BM_HTTP_Format_Default_UserAgent(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_t out;
        dc_string_init(&out);
        dc_http_format_default_user_agent(&out);
        total_bytes += dc_string_length(&out);
        benchmark::DoNotOptimize(dc_string_length(&out));
        dc_string_free(&out);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_HTTP_Format_Default_UserAgent);

static void BM_HTTP_UserAgent_Validate(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        int ok = dc_http_user_agent_is_valid(kUserAgentValue);
        benchmark::DoNotOptimize(ok);
        total_bytes += strlen(kUserAgentValue);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_HTTP_UserAgent_Validate);

static void BM_HTTP_ContentType_Allowed(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        int ok = dc_http_content_type_is_allowed(kContentTypeValue);
        benchmark::DoNotOptimize(ok);
        total_bytes += strlen(kContentTypeValue);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_HTTP_ContentType_Allowed);

static void BM_HTTP_Format_Auth(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_t out;
        dc_string_init(&out);
        dc_http_format_auth_header(DC_HTTP_AUTH_BOT, "token123", &out);
        total_bytes += dc_string_length(&out);
        benchmark::DoNotOptimize(dc_string_length(&out));
        dc_string_free(&out);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_HTTP_Format_Auth);

static void BM_HTTP_Format_Auth_Bearer(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_t out;
        dc_string_init(&out);
        dc_http_format_auth_header(DC_HTTP_AUTH_BEARER, "token123", &out);
        total_bytes += dc_string_length(&out);
        benchmark::DoNotOptimize(dc_string_length(&out));
        dc_string_free(&out);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_HTTP_Format_Auth_Bearer);

static void BM_HTTP_Query_Bool(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_t q;
        dc_string_init(&q);
        dc_http_append_query_bool(&q, "with_counts", 1, DC_HTTP_BOOL_TRUE_FALSE);
        dc_http_append_query_bool(&q, "limit", 0, DC_HTTP_BOOL_ONE_ZERO);
        total_bytes += dc_string_length(&q);
        benchmark::DoNotOptimize(dc_string_length(&q));
        dc_string_free(&q);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_HTTP_Query_Bool);

static void BM_HTTP_JSON_Validate(benchmark::State& state) {
    const char* json = "{\"content\":\"hello\",\"tts\":false,\"embeds\":[]}";
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_status_t st = dc_http_validate_json_body(json, 0);
        benchmark::DoNotOptimize(st);
        total_bytes += strlen(json);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_HTTP_JSON_Validate);

static void BM_HTTP_RateLimit_Parse(benchmark::State& state) {
    dc_http_rate_limit_t rl;
    if (dc_http_rate_limit_init(&rl) != DC_OK) {
        state.SkipWithError("dc_http_rate_limit_init failed");
        return;
    }
    for (auto _ : state) {
        dc_status_t st = dc_http_rate_limit_parse(dc_bench_get_header, (void*)kRateLimitHeaders, &rl);
        benchmark::DoNotOptimize(st);
        benchmark::DoNotOptimize(rl.remaining);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    dc_http_rate_limit_free(&rl);
}
BENCHMARK(BM_HTTP_RateLimit_Parse);

static void BM_HTTP_RateLimit_Response_Parse(benchmark::State& state) {
    dc_http_rate_limit_response_t rl;
    if (dc_http_rate_limit_response_init(&rl) != DC_OK) {
        state.SkipWithError("dc_http_rate_limit_response_init failed");
        return;
    }
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_status_t st = dc_http_rate_limit_response_parse(kRateLimitBody, 0, &rl);
        benchmark::DoNotOptimize(st);
        benchmark::DoNotOptimize(rl.retry_after);
        total_bytes += strlen(kRateLimitBody);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    dc_http_rate_limit_response_free(&rl);
}
BENCHMARK(BM_HTTP_RateLimit_Response_Parse);

static void BM_HTTP_Error_Parse(benchmark::State& state) {
    const char* err_json =
        "{\"code\":50035,\"message\":\"Invalid Form Body\","
        "\"errors\":{\"content\":{\"_errors\":[{\"code\":\"BASE_TYPE_REQUIRED\","
        "\"message\":\"This field is required\"}]}}}";
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_http_error_t err;
        dc_http_error_init(&err);
        dc_http_error_parse(err_json, 0, &err);
        benchmark::DoNotOptimize(err.code);
        dc_http_error_free(&err);
        total_bytes += strlen(err_json);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_HTTP_Error_Parse);

static void BM_HTTP_Request_Set_JSON(benchmark::State& state) {
    const char* json = "{\"content\":\"hello\"}";
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_http_request_t req;
        dc_http_request_init(&req);
        dc_http_request_set_json_body(&req, json);
        total_bytes += req.body.length;
        benchmark::DoNotOptimize(req.body.length);
        dc_http_request_free(&req);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_HTTP_Request_Set_JSON);

static void BM_HTTP_Add_Headers(benchmark::State& state) {
    const size_t count = static_cast<size_t>(state.range(0));
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_http_request_t req;
        dc_http_request_init(&req);
        for (size_t i = 0; i < count; i++) {
            char name[32];
            snprintf(name, sizeof(name), "X-Test-%u", (unsigned)i);
            dc_http_request_add_header(&req, name, "value");
            total_bytes += strlen(name) + 5; // name + "value"
        }
        benchmark::DoNotOptimize(req.headers.length);
        dc_http_request_free(&req);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * count));
}
BENCHMARK(BM_HTTP_Add_Headers)->Range(4, 256);

BENCHMARK_MAIN();
