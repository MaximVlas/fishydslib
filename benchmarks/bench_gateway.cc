/**
 * @file bench_gateway.cc
 * @brief Gateway benchmarks
 */

#include <benchmark/benchmark.h>
#include <string.h>

extern "C" {
#include "gw/dc_gateway.h"
#include "gw/dc_events.h"
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

static const char* kEventNames[] = {
    "MESSAGE_CREATE",
    "READY",
    "GUILD_CREATE",
    "THREAD_CREATE",
    "THREAD_UPDATE",
    "THREAD_DELETE",
    "THREAD_LIST_SYNC",
    "THREAD_MEMBER_UPDATE",
    "THREAD_MEMBERS_UPDATE",
    "PRESENCE_UPDATE",
    "VOICE_STATE_UPDATE",
    "UNKNOWN_EVENT_NAME"
};
static const size_t kEventNameCount = sizeof(kEventNames) / sizeof(kEventNames[0]);

static const char* kReadyJson = R"json({
    "v":10,
    "user":{"id":"123456789012345678","username":"testbot","discriminator":"0"},
    "guilds":[
        {"id":"111","unavailable":true},
        {"id":"222","unavailable":true},
        {"id":"333","unavailable":true}
    ],
    "session_id":"session_abc123",
    "resume_gateway_url":"wss://gateway.discord.gg/?v=10&encoding=json",
    "shard":[0,1],
    "application":{"id":"123456789012345678","flags":0}
})json";

static const char* kMessageCreateJson = R"json({
    "id":"999",
    "channel_id":"1000",
    "author":{"id":"123456789012345678","username":"alice"},
    "content":"hello from gateway benchmarks",
    "timestamp":"2024-01-15T12:00:00.000Z",
    "tts":false,
    "mention_everyone":false,
    "mentions":[],
    "mention_roles":[],
    "attachments":[],
    "embeds":[],
    "pinned":false,
    "type":0
})json";

static const char* kMessageCreateFullJson = R"json({
    "id":"999",
    "channel_id":"1000",
    "guild_id":"555",
    "author":{"id":"123456789012345678","username":"alice"},
    "member":{"nick":"Alice","roles":["111","222"],"joined_at":"2023-01-01T00:00:00.000Z","deaf":false,"mute":false},
    "content":"hello from gateway",
    "timestamp":"2024-01-15T12:00:00.000Z",
    "tts":false,
    "mention_everyone":false,
    "mentions":[],
    "mention_roles":[],
    "attachments":[],
    "embeds":[],
    "pinned":false,
    "type":0
})json";

static const char* kThreadChannelJson = R"json({
    "id":"900",
    "type":11,
    "name":"test-thread",
    "thread_metadata":{
        "archived":false,
        "auto_archive_duration":60,
        "archive_timestamp":"2024-01-01T00:00:00.000Z",
        "locked":false
    }
})json";

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

static void BM_Gateway_EventKindFromName(benchmark::State& state) {
    for (auto _ : state) {
        for (size_t i = 0; i < kEventNameCount; i++) {
            dc_gateway_event_kind_t kind = dc_gateway_event_kind_from_name(kEventNames[i]);
            benchmark::DoNotOptimize(kind);
        }
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(kEventNameCount));
}
BENCHMARK(BM_Gateway_EventKindFromName);

static void BM_Gateway_EventIsThread(benchmark::State& state) {
    for (auto _ : state) {
        for (size_t i = 0; i < kEventNameCount; i++) {
            int ok = dc_gateway_event_is_thread_event(kEventNames[i]);
            benchmark::DoNotOptimize(ok);
        }
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(kEventNameCount));
}
BENCHMARK(BM_Gateway_EventIsThread);

static void BM_Gateway_ParseReady(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_gateway_ready_t ready;
        dc_gateway_ready_init(&ready);
        dc_status_t st = dc_gateway_event_parse_ready(kReadyJson, &ready);
        benchmark::DoNotOptimize(st);
        benchmark::DoNotOptimize(ready.v);
        dc_gateway_ready_free(&ready);
        total_bytes += strlen(kReadyJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Gateway_ParseReady);

static void BM_Gateway_ParseMessageCreate(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_message_t message;
        dc_message_init(&message);
        dc_status_t st = dc_gateway_event_parse_message_create(kMessageCreateJson, &message);
        benchmark::DoNotOptimize(st);
        benchmark::DoNotOptimize(message.id);
        dc_message_free(&message);
        total_bytes += strlen(kMessageCreateJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Gateway_ParseMessageCreate);

static void BM_Gateway_ParseMessageCreateFull(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_gateway_message_create_t msg;
        dc_gateway_message_create_init(&msg);
        dc_status_t st = dc_gateway_event_parse_message_create_full(kMessageCreateFullJson, &msg);
        benchmark::DoNotOptimize(st);
        benchmark::DoNotOptimize(msg.message.id);
        dc_gateway_message_create_free(&msg);
        total_bytes += strlen(kMessageCreateFullJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Gateway_ParseMessageCreateFull);

static void BM_Gateway_ParseThreadChannel(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_channel_t channel;
        dc_channel_init(&channel);
        dc_status_t st = dc_gateway_event_parse_thread_channel(kThreadChannelJson, &channel);
        benchmark::DoNotOptimize(st);
        benchmark::DoNotOptimize(channel.id);
        dc_channel_free(&channel);
        total_bytes += strlen(kThreadChannelJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Gateway_ParseThreadChannel);

BENCHMARK_MAIN();
