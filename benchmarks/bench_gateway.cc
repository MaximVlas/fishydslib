/**
 * @file bench_gateway.cc
 * @brief Gateway benchmarks
 */

#include <benchmark/benchmark.h>
#include <string.h>

extern "C" {
#include "gw/dc_gateway.h"
#include "core/dc_status.h"
}

static dc_gateway_config_t bench_gateway_default_config(void) {
    dc_gateway_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.token = "token123";
    cfg.intents = 0;
    cfg.user_agent = "DiscordBot (https://example.com, 0.1.0) fishydslib";
    cfg.heartbeat_timeout_ms = 0;
    cfg.connect_timeout_ms = 0;
    cfg.enable_compression = 0;
    cfg.enable_payload_compression = 0;
    return cfg;
}

static void BM_Gateway_CloseCode_String(benchmark::State& state) {
    const int codes[] = {
        DC_GATEWAY_CLOSE_UNKNOWN_ERROR,
        DC_GATEWAY_CLOSE_AUTHENTICATION_FAILED,
        DC_GATEWAY_CLOSE_INVALID_INTENTS,
        DC_GATEWAY_CLOSE_DISALLOWED_INTENTS,
        9999
    };
    size_t total_bytes = 0;
    for (auto _ : state) {
        for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
            const char* s = dc_gateway_close_code_string(codes[i]);
            total_bytes += strlen(s);
            benchmark::DoNotOptimize(s);
        }
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
}
BENCHMARK(BM_Gateway_CloseCode_String);

static void BM_Gateway_CloseCode_Reconnect(benchmark::State& state) {
    const int codes[] = {
        DC_GATEWAY_CLOSE_AUTHENTICATION_FAILED,
        DC_GATEWAY_CLOSE_INVALID_SHARD,
        DC_GATEWAY_CLOSE_SHARDING_REQUIRED,
        DC_GATEWAY_CLOSE_UNKNOWN_ERROR,
        1000
    };
    for (auto _ : state) {
        for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
            int ok = dc_gateway_close_code_should_reconnect(codes[i]);
            benchmark::DoNotOptimize(ok);
        }
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 5);
}
BENCHMARK(BM_Gateway_CloseCode_Reconnect);

static void BM_Gateway_ClientCreateFree(benchmark::State& state) {
    dc_gateway_config_t cfg = bench_gateway_default_config();
    for (auto _ : state) {
        dc_gateway_client_t* client = NULL;
        dc_status_t st = dc_gateway_client_create(&cfg, &client);
        benchmark::DoNotOptimize(st);
        if (st != DC_OK) {
            state.SkipWithError("dc_gateway_client_create failed");
            return;
        }
        dc_gateway_client_free(client);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Gateway_ClientCreateFree);

static void BM_Gateway_ClientCreateFree_Compress(benchmark::State& state) {
    dc_gateway_config_t cfg = bench_gateway_default_config();
    cfg.enable_compression = 1;
    for (auto _ : state) {
        dc_gateway_client_t* client = NULL;
        dc_status_t st = dc_gateway_client_create(&cfg, &client);
        benchmark::DoNotOptimize(st);
        if (st != DC_OK) {
            state.SkipWithError("dc_gateway_client_create failed");
            return;
        }
        dc_gateway_client_free(client);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Gateway_ClientCreateFree_Compress);

BENCHMARK_MAIN();
