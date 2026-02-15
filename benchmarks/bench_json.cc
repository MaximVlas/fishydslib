#include <benchmark/benchmark.h>
#include <cstring>

extern "C" {
#include "json/dc_json.h"
#include "core/dc_string.h"
#include "model/dc_user.h"
#include "model/dc_channel.h"
#include "model/dc_message.h"
#include "model/dc_role.h"
#include "model/dc_guild.h"
#include "model/dc_guild_member.h"
}

static const char* kSmallJson = "{\"id\":\"123456789012345678\",\"name\":\"test\",\"value\":42}";
static const char* kPrimitiveJson = R"json({
    "name":"test",
    "count":42,
    "uval":9223372036854775808,
    "flag":true,
    "ratio":3.14159,
    "nested":{"inner":1},
    "arr":[1,2,3,4],
    "snowflake":"123456789012345678",
    "perm":"2048"
})json";
static const char* kOptionalJson = R"json({
    "opt_str":"value",
    "opt_i64":123,
    "opt_bool":true,
    "opt_double":2.5,
    "null_str":null,
    "null_i64":null,
    "null_bool":null,
    "null_double":null
})json";
static const char* kUserJson = R"json({
    "id":"123456789012345678",
    "username":"alice",
    "discriminator":"1234",
    "global_name":"Alice",
    "avatar":"abc123",
    "banner":"bannerhash",
    "accent_color":16711680,
    "locale":"en-US",
    "email":"alice@example.com",
    "flags":64,
    "premium_type":2,
    "public_flags":1,
    "avatar_decoration":"decoration",
    "bot":true,
    "system":false,
    "mfa_enabled":true,
    "verified":true
})json";
static const char* kChannelJson = R"json({
    "id":"555",
    "type":0,
    "name":"general",
    "topic":"benchmarks",
    "last_pin_timestamp":"2024-01-01T00:00:00.000Z",
    "rate_limit_per_user":5,
    "flags":64,
    "permissions":"1048576",
    "thread_metadata":{
        "archived":false,
        "auto_archive_duration":60,
        "archive_timestamp":"2024-01-01T00:00:00.000Z",
        "locked":false,
        "invitable":true,
        "create_timestamp":null
    },
    "available_tags":[
        {"id":"1","name":"tag1","moderated":false,"emoji_id":null,"emoji_name":null}
    ],
    "applied_tags":["1","2","3"],
    "default_reaction_emoji":{"emoji_id":"2","emoji_name":"smile"}
})json";
static const char* kMessageJson = R"json({
    "id":"999",
    "channel_id":"1000",
    "author":{"id":"123456789012345678","username":"alice"},
    "content":"hello from benchmarks",
    "timestamp":"2024-01-01T00:00:00.000Z",
    "edited_timestamp":"2024-01-01T01:00:00.000Z",
    "tts":false,
    "mention_everyone":false,
    "pinned":false,
    "type":0,
    "flags":64,
    "webhook_id":"123456789012345679",
    "application_id":"123456789012345680",
    "mention_roles":["111","222","333","444"],
    "thread":{"id":"555","type":11,"name":"bench-thread"}
})json";

static const char* kRoleJson = R"json({
    "id":"111222333444555666",
    "name":"Moderator",
    "color":3447003,
    "hoist":true,
    "icon":null,
    "unicode_emoji":null,
    "position":5,
    "permissions":"1099511627775",
    "managed":false,
    "mentionable":true,
    "flags":0,
    "tags":{"bot_id":"123456789012345678"}
})json";

static const char* kGuildMemberJson = R"json({
    "user":{"id":"123456789012345678","username":"alice","discriminator":"0"},
    "nick":"Alice",
    "avatar":null,
    "roles":["111","222","333"],
    "joined_at":"2023-06-15T10:30:00.000Z",
    "premium_since":null,
    "deaf":false,
    "mute":false,
    "flags":0,
    "pending":false,
    "permissions":"1099511627775",
    "communication_disabled_until":null
})json";

static const char* kGuildJson = R"json({
    "id":"999888777666555444",
    "name":"Test Server",
    "icon":"iconhash123",
    "icon_hash":null,
    "splash":null,
    "discovery_splash":null,
    "owner_id":"123456789012345678",
    "afk_channel_id":"111",
    "afk_timeout":300,
    "verification_level":2,
    "default_message_notifications":1,
    "explicit_content_filter":2,
    "mfa_level":1,
    "system_channel_id":"222",
    "system_channel_flags":0,
    "rules_channel_id":"333",
    "vanity_url_code":null,
    "description":"A test server for benchmarks",
    "banner":null,
    "premium_tier":2,
    "premium_subscription_count":15,
    "preferred_locale":"en-US",
    "public_updates_channel_id":"444",
    "nsfw_level":0,
    "premium_progress_bar_enabled":true
})json";

static const char* kUserWithSubObjectsJson = R"json({
    "id":"123456789012345678",
    "username":"alice",
    "discriminator":"0",
    "global_name":"Alice",
    "avatar":"abc123",
    "banner":"bannerhash",
    "accent_color":16711680,
    "locale":"en-US",
    "flags":64,
    "premium_type":2,
    "public_flags":256,
    "avatar_decoration_data":{"asset":"a_decohash","sku_id":"999888777666555444"},
    "collectibles":{"nameplate":{"sku_id":"111222333444555666","asset":"np_asset","label":"Cool Plate","palette":"#FF0000"}},
    "primary_guild":{"identity_guild_id":"999888777666555444","identity_enabled":true,"tag":"TEST","badge":"badgehash"},
    "bot":false,
    "system":false,
    "mfa_enabled":true,
    "verified":true
})json";

static dc_status_t dc_bench_fill_user(dc_user_t* user) {
    dc_status_t st = dc_user_init(user);
    if (st != DC_OK) return st;
    user->id = 123456789012345678ULL;
    st = dc_string_set_cstr(&user->username, "alice");
    if (st != DC_OK) return st;
    st = dc_string_set_cstr(&user->discriminator, "1234");
    if (st != DC_OK) return st;
    st = dc_string_set_cstr(&user->global_name, "Alice");
    if (st != DC_OK) return st;
    st = dc_string_set_cstr(&user->avatar, "abc123");
    if (st != DC_OK) return st;
    st = dc_string_set_cstr(&user->banner, "bannerhash");
    if (st != DC_OK) return st;
    user->accent_color = 16711680;
    st = dc_string_set_cstr(&user->locale, "en-US");
    if (st != DC_OK) return st;
    st = dc_string_set_cstr(&user->email, "alice@example.com");
    if (st != DC_OK) return st;
    user->flags = 64;
    user->premium_type = DC_USER_PREMIUM_NITRO;
    user->public_flags = 1;
    st = dc_string_set_cstr(&user->avatar_decoration, "decoration");
    if (st != DC_OK) return st;
    user->bot = 1;
    user->system = 0;
    user->mfa_enabled = 1;
    user->verified = 1;
    return DC_OK;
}

static dc_status_t dc_bench_fill_channel(dc_channel_t* channel) {
    channel->id = 555ULL;
    channel->type = DC_CHANNEL_TYPE_PUBLIC_THREAD;
    return dc_string_set_cstr(&channel->name, "bench-thread");
}

static dc_status_t dc_bench_fill_message(dc_message_t* message) {
    dc_status_t st = dc_message_init(message);
    if (st != DC_OK) return st;
    message->id = 999ULL;
    message->channel_id = 1000ULL;
    st = dc_string_set_cstr(&message->content, "hello from benchmarks");
    if (st != DC_OK) return st;
    st = dc_string_set_cstr(&message->timestamp, "2024-01-01T00:00:00.000Z");
    if (st != DC_OK) return st;
    message->edited_timestamp.is_null = 0;
    st = dc_string_set_cstr(&message->edited_timestamp.value, "2024-01-01T01:00:00.000Z");
    if (st != DC_OK) return st;
    message->tts = 0;
    message->mention_everyone = 0;
    message->pinned = 0;
    message->type = DC_MESSAGE_TYPE_DEFAULT;
    message->flags = 64;
    message->webhook_id.is_set = 1;
    message->webhook_id.value = 123456789012345679ULL;
    message->application_id.is_set = 1;
    message->application_id.value = 123456789012345680ULL;
    {
        const dc_snowflake_t roles[] = {
            111ULL, 222ULL, 333ULL, 444ULL
        };
        st = dc_vec_reserve(&message->mention_roles, sizeof(roles) / sizeof(roles[0]));
        if (st != DC_OK) return st;
        for (size_t i = 0; i < sizeof(roles) / sizeof(roles[0]); i++) {
            st = dc_vec_push(&message->mention_roles, &roles[i]);
            if (st != DC_OK) return st;
        }
    }
    message->has_thread = 1;
    st = dc_string_set_cstr(&message->author.username, "alice");
    if (st != DC_OK) return st;
    message->author.id = 123456789012345678ULL;
    st = dc_string_set_cstr(&message->author.discriminator, "1234");
    if (st != DC_OK) return st;
    st = dc_bench_fill_channel(&message->thread);
    return st;
}

static void BM_JSON_Parse(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_json_doc_t doc;
        dc_status_t st = dc_json_parse(kSmallJson, &doc);
        benchmark::DoNotOptimize(st);
        dc_json_doc_free(&doc);
        total_bytes += strlen(kSmallJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Parse);

static void BM_JSON_Parse_Relaxed(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_json_doc_t doc;
        dc_status_t st = dc_json_parse_relaxed(kSmallJson, &doc);
        benchmark::DoNotOptimize(st);
        dc_json_doc_free(&doc);
        total_bytes += strlen(kSmallJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Parse_Relaxed);

static void BM_JSON_Parse_Buffer(benchmark::State& state) {
    size_t len = strlen(kSmallJson);
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_json_doc_t doc;
        dc_status_t st = dc_json_parse_buffer(kSmallJson, len, &doc);
        benchmark::DoNotOptimize(st);
        dc_json_doc_free(&doc);
        total_bytes += len;
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Parse_Buffer);

static void BM_JSON_Parse_Buffer_Relaxed(benchmark::State& state) {
    size_t len = strlen(kSmallJson);
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_json_doc_t doc;
        dc_status_t st = dc_json_parse_buffer_relaxed(kSmallJson, len, &doc);
        benchmark::DoNotOptimize(st);
        dc_json_doc_free(&doc);
        total_bytes += len;
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Parse_Buffer_Relaxed);

static void BM_JSON_Get_Snowflake(benchmark::State& state) {
    dc_json_doc_t doc;
    dc_json_parse(kSmallJson, &doc);
    size_t total_bytes = 0;
    for (auto _ : state) {
        uint64_t id = 0;
        dc_status_t st = dc_json_get_snowflake(doc.root, "id", &id);
        benchmark::DoNotOptimize(st);
        benchmark::DoNotOptimize(id);
        total_bytes += sizeof(uint64_t);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    dc_json_doc_free(&doc);
}
BENCHMARK(BM_JSON_Get_Snowflake);

static void BM_JSON_Get_Primitives(benchmark::State& state) {
    dc_json_doc_t doc;
    if (dc_json_parse(kPrimitiveJson, &doc) != DC_OK) {
        state.SkipWithError("dc_json_parse failed");
        return;
    }
    size_t total_items = 0;
    for (auto _ : state) {
        const char* name = NULL;
        int64_t count = 0;
        uint64_t uval = 0;
        int flag = 0;
        double ratio = 0.0;
        yyjson_val* nested = NULL;
        yyjson_val* arr = NULL;
        uint64_t snowflake = 0;
        uint64_t perm = 0;
        benchmark::DoNotOptimize(dc_json_get_string(doc.root, "name", &name));
        benchmark::DoNotOptimize(dc_json_get_int64(doc.root, "count", &count));
        benchmark::DoNotOptimize(dc_json_get_uint64(doc.root, "uval", &uval));
        benchmark::DoNotOptimize(dc_json_get_bool(doc.root, "flag", &flag));
        benchmark::DoNotOptimize(dc_json_get_double(doc.root, "ratio", &ratio));
        benchmark::DoNotOptimize(dc_json_get_object(doc.root, "nested", &nested));
        benchmark::DoNotOptimize(dc_json_get_array(doc.root, "arr", &arr));
        benchmark::DoNotOptimize(dc_json_get_snowflake(doc.root, "snowflake", &snowflake));
        benchmark::DoNotOptimize(dc_json_get_permission(doc.root, "perm", &perm));
        benchmark::DoNotOptimize(name);
        benchmark::DoNotOptimize(count);
        benchmark::DoNotOptimize(uval);
        benchmark::DoNotOptimize(flag);
        benchmark::DoNotOptimize(ratio);
        benchmark::DoNotOptimize(snowflake);
        benchmark::DoNotOptimize(perm);
        total_items += 9;
    }
    state.SetItemsProcessed(static_cast<int64_t>(total_items));
    dc_json_doc_free(&doc);
}
BENCHMARK(BM_JSON_Get_Primitives);

static void BM_JSON_Get_Optional_Nullable(benchmark::State& state) {
    dc_json_doc_t doc;
    if (dc_json_parse(kOptionalJson, &doc) != DC_OK) {
        state.SkipWithError("dc_json_parse failed");
        return;
    }
    size_t total_items = 0;
    for (auto _ : state) {
        const char* opt_str = NULL;
        int64_t opt_i64 = 0;
        int opt_bool = 0;
        double opt_double = 0.0;
        dc_optional_cstr_t opt_str_val = {0};
        dc_optional_i64_t opt_i64_val = {0};
        dc_optional_int_t opt_bool_val = {0};
        dc_optional_double_t opt_double_val = {0};
        dc_nullable_cstr_t null_str_val = {0};
        dc_nullable_i64_t null_i64_val = {0};
        dc_nullable_int_t null_bool_val = {0};
        dc_nullable_double_t null_double_val = {0};
        benchmark::DoNotOptimize(dc_json_get_string_opt(doc.root, "opt_str", &opt_str, ""));
        benchmark::DoNotOptimize(dc_json_get_int64_opt(doc.root, "opt_i64", &opt_i64, 0));
        benchmark::DoNotOptimize(dc_json_get_bool_opt(doc.root, "opt_bool", &opt_bool, 0));
        benchmark::DoNotOptimize(dc_json_get_double_opt(doc.root, "opt_double", &opt_double, 0.0));
        benchmark::DoNotOptimize(dc_json_get_string_optional(doc.root, "opt_str", &opt_str_val));
        benchmark::DoNotOptimize(dc_json_get_int64_optional(doc.root, "opt_i64", &opt_i64_val));
        benchmark::DoNotOptimize(dc_json_get_bool_optional(doc.root, "opt_bool", &opt_bool_val));
        benchmark::DoNotOptimize(dc_json_get_double_optional(doc.root, "opt_double", &opt_double_val));
        benchmark::DoNotOptimize(dc_json_get_string_nullable(doc.root, "null_str", &null_str_val));
        benchmark::DoNotOptimize(dc_json_get_int64_nullable(doc.root, "null_i64", &null_i64_val));
        benchmark::DoNotOptimize(dc_json_get_bool_nullable(doc.root, "null_bool", &null_bool_val));
        benchmark::DoNotOptimize(dc_json_get_double_nullable(doc.root, "null_double", &null_double_val));
        benchmark::DoNotOptimize(opt_str);
        benchmark::DoNotOptimize(opt_i64);
        benchmark::DoNotOptimize(opt_bool);
        benchmark::DoNotOptimize(opt_double);
        total_items += 12;
    }
    state.SetItemsProcessed(static_cast<int64_t>(total_items));
    dc_json_doc_free(&doc);
}
BENCHMARK(BM_JSON_Get_Optional_Nullable);

static void BM_JSON_Mut_Serialize(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_json_mut_doc_t doc;
        dc_json_mut_doc_create(&doc);
        dc_json_mut_set_string(&doc, doc.root, "name", "test");
        dc_json_mut_set_int64(&doc, doc.root, "value", 42);
        dc_json_mut_set_snowflake(&doc, doc.root, "id", 123456789012345678ULL);

        dc_string_t out;
        dc_string_init(&out);
        dc_json_mut_doc_serialize(&doc, &out);
        size_t len = dc_string_length(&out);
        benchmark::DoNotOptimize(len);
        total_bytes += len;
        dc_string_free(&out);
        dc_json_mut_doc_free(&doc);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Mut_Serialize);

static void BM_JSON_Model_User_Parse(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_user_t user;
        dc_status_t st = dc_user_from_json(kUserJson, &user);
        benchmark::DoNotOptimize(st);
        if (st == DC_OK) {
            benchmark::DoNotOptimize(user.id);
            dc_user_free(&user);
        }
        total_bytes += strlen(kUserJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Model_User_Parse);

static void BM_JSON_Model_Channel_Parse(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_channel_t channel;
        dc_status_t st = dc_channel_from_json(kChannelJson, &channel);
        benchmark::DoNotOptimize(st);
        if (st == DC_OK) {
            benchmark::DoNotOptimize(channel.id);
            dc_channel_free(&channel);
        }
        total_bytes += strlen(kChannelJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Model_Channel_Parse);

static void BM_JSON_Model_Message_Parse(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_message_t message;
        dc_status_t st = dc_message_from_json(kMessageJson, &message);
        benchmark::DoNotOptimize(st);
        if (st == DC_OK) {
            benchmark::DoNotOptimize(message.id);
            dc_message_free(&message);
        }
        total_bytes += strlen(kMessageJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Model_Message_Parse);

static void BM_JSON_Model_User_Serialize(benchmark::State& state) {
    dc_user_t user;
    if (dc_bench_fill_user(&user) != DC_OK) {
        dc_user_free(&user);
        state.SkipWithError("dc_bench_fill_user failed");
        return;
    }
    dc_string_t out;
    if (dc_string_init(&out) != DC_OK) {
        dc_user_free(&user);
        state.SkipWithError("dc_string_init failed");
        return;
    }
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_clear(&out);
        dc_status_t st = dc_user_to_json(&user, &out);
        benchmark::DoNotOptimize(st);
        size_t len = dc_string_length(&out);
        benchmark::DoNotOptimize(len);
        total_bytes += len;
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    dc_string_free(&out);
    dc_user_free(&user);
}
BENCHMARK(BM_JSON_Model_User_Serialize);

static void BM_JSON_Model_Message_Serialize(benchmark::State& state) {
    dc_message_t message;
    if (dc_bench_fill_message(&message) != DC_OK) {
        dc_message_free(&message);
        state.SkipWithError("dc_bench_fill_message failed");
        return;
    }
    dc_string_t out;
    if (dc_string_init(&out) != DC_OK) {
        dc_message_free(&message);
        state.SkipWithError("dc_string_init failed");
        return;
    }
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_clear(&out);
        dc_status_t st = dc_message_to_json(&message, &out);
        benchmark::DoNotOptimize(st);
        size_t len = dc_string_length(&out);
        benchmark::DoNotOptimize(len);
        total_bytes += len;
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    dc_string_free(&out);
    dc_message_free(&message);
}
BENCHMARK(BM_JSON_Model_Message_Serialize);

static void BM_JSON_Model_Role_Parse(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_role_t role;
        dc_status_t st = dc_role_from_json(kRoleJson, &role);
        benchmark::DoNotOptimize(st);
        if (st == DC_OK) {
            benchmark::DoNotOptimize(role.id);
            dc_role_free(&role);
        }
        total_bytes += strlen(kRoleJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Model_Role_Parse);

static void BM_JSON_Model_Role_Serialize(benchmark::State& state) {
    dc_role_t role;
    dc_role_init(&role);
    role.id = 111222333444555666ULL;
    dc_string_set_cstr(&role.name, "Moderator");
    role.color = 3447003;
    role.hoist = 1;
    role.position = 5;
    role.permissions = 1099511627775ULL;
    role.mentionable = 1;
    dc_string_t out;
    dc_string_init(&out);
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_clear(&out);
        dc_status_t st = dc_role_to_json(&role, &out);
        benchmark::DoNotOptimize(st);
        total_bytes += dc_string_length(&out);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    dc_string_free(&out);
    dc_role_free(&role);
}
BENCHMARK(BM_JSON_Model_Role_Serialize);

static void BM_JSON_Model_GuildMember_Parse(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_guild_member_t member;
        dc_status_t st = dc_guild_member_from_json(kGuildMemberJson, &member);
        benchmark::DoNotOptimize(st);
        if (st == DC_OK) {
            benchmark::DoNotOptimize(member.has_user);
            dc_guild_member_free(&member);
        }
        total_bytes += strlen(kGuildMemberJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Model_GuildMember_Parse);

static void BM_JSON_Model_GuildMember_Serialize(benchmark::State& state) {
    dc_guild_member_t member;
    dc_guild_member_init(&member);
    member.has_user = 1;
    member.user.id = 123456789012345678ULL;
    dc_string_set_cstr(&member.user.username, "alice");
    member.nick.is_null = 0;
    dc_string_set_cstr(&member.nick.value, "Alice");
    dc_string_set_cstr(&member.joined_at, "2023-06-15T10:30:00.000Z");
    dc_string_t out;
    dc_string_init(&out);
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_clear(&out);
        dc_status_t st = dc_guild_member_to_json(&member, &out);
        benchmark::DoNotOptimize(st);
        total_bytes += dc_string_length(&out);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    dc_string_free(&out);
    dc_guild_member_free(&member);
}
BENCHMARK(BM_JSON_Model_GuildMember_Serialize);

static void BM_JSON_Model_Guild_Parse(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_guild_t guild;
        dc_status_t st = dc_guild_from_json(kGuildJson, &guild);
        benchmark::DoNotOptimize(st);
        if (st == DC_OK) {
            benchmark::DoNotOptimize(guild.id);
            dc_guild_free(&guild);
        }
        total_bytes += strlen(kGuildJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Model_Guild_Parse);

static void BM_JSON_Model_Guild_Serialize(benchmark::State& state) {
    dc_guild_t guild;
    dc_guild_init(&guild);
    guild.id = 999888777666555444ULL;
    dc_string_set_cstr(&guild.name, "Test Server");
    guild.icon.is_null = 0;
    dc_string_set_cstr(&guild.icon.value, "iconhash123");
    guild.verification_level = 2;
    guild.default_message_notifications = 1;
    guild.explicit_content_filter = 2;
    guild.mfa_level = 1;
    guild.premium_tier = 2;
    dc_string_set_cstr(&guild.preferred_locale, "en-US");
    guild.premium_progress_bar_enabled = 1;
    dc_string_t out;
    dc_string_init(&out);
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_clear(&out);
        dc_status_t st = dc_guild_to_json(&guild, &out);
        benchmark::DoNotOptimize(st);
        total_bytes += dc_string_length(&out);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    dc_string_free(&out);
    dc_guild_free(&guild);
}
BENCHMARK(BM_JSON_Model_Guild_Serialize);

static void BM_JSON_Model_User_WithSubObjects_Parse(benchmark::State& state) {
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_user_t user;
        dc_status_t st = dc_user_from_json(kUserWithSubObjectsJson, &user);
        benchmark::DoNotOptimize(st);
        if (st == DC_OK) {
            benchmark::DoNotOptimize(user.has_avatar_decoration_data);
            benchmark::DoNotOptimize(user.has_collectibles);
            benchmark::DoNotOptimize(user.has_primary_guild);
            dc_user_free(&user);
        }
        total_bytes += strlen(kUserWithSubObjectsJson);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Model_User_WithSubObjects_Parse);

static void BM_JSON_Model_User_WithSubObjects_Serialize(benchmark::State& state) {
    dc_user_t user;
    dc_user_init(&user);
    user.id = 123456789012345678ULL;
    dc_string_set_cstr(&user.username, "alice");
    dc_string_set_cstr(&user.global_name, "Alice");
    dc_string_set_cstr(&user.avatar, "abc123");
    user.flags = 64;
    user.public_flags = 256;
    user.has_avatar_decoration_data = 1;
    dc_string_set_cstr(&user.avatar_decoration_data.asset, "a_decohash");
    user.avatar_decoration_data.sku_id = 999888777666555444ULL;
    user.has_collectibles = 1;
    user.collectibles.has_nameplate = 1;
    user.collectibles.nameplate.sku_id = 111222333444555666ULL;
    dc_string_set_cstr(&user.collectibles.nameplate.asset, "np_asset");
    dc_string_set_cstr(&user.collectibles.nameplate.label, "Cool Plate");
    dc_string_set_cstr(&user.collectibles.nameplate.palette, "#FF0000");
    user.has_primary_guild = 1;
    user.primary_guild.identity_guild_id.is_null = 0;
    user.primary_guild.identity_guild_id.value = 999888777666555444ULL;
    user.primary_guild.identity_enabled.is_null = 0;
    user.primary_guild.identity_enabled.value = 1;
    user.primary_guild.tag.is_null = 0;
    dc_string_set_cstr(&user.primary_guild.tag.value, "TEST");
    user.primary_guild.badge.is_null = 0;
    dc_string_set_cstr(&user.primary_guild.badge.value, "badgehash");
    dc_string_t out;
    dc_string_init(&out);
    size_t total_bytes = 0;
    for (auto _ : state) {
        dc_string_clear(&out);
        dc_status_t st = dc_user_to_json(&user, &out);
        benchmark::DoNotOptimize(st);
        total_bytes += dc_string_length(&out);
        benchmark::DoNotOptimize(dc_string_length(&out));
    }
    state.SetBytesProcessed(static_cast<int64_t>(total_bytes));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    dc_string_free(&out);
    dc_user_free(&user);
}
BENCHMARK(BM_JSON_Model_User_WithSubObjects_Serialize);

static void BM_JSON_Model_InitFree_Role(benchmark::State& state) {
    for (auto _ : state) {
        dc_role_t role;
        dc_role_init(&role);
        benchmark::DoNotOptimize(role.id);
        dc_role_free(&role);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Model_InitFree_Role);

static void BM_JSON_Model_InitFree_Guild(benchmark::State& state) {
    for (auto _ : state) {
        dc_guild_t guild;
        dc_guild_init(&guild);
        benchmark::DoNotOptimize(guild.id);
        dc_guild_free(&guild);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Model_InitFree_Guild);

static void BM_JSON_Model_InitFree_GuildMember(benchmark::State& state) {
    for (auto _ : state) {
        dc_guild_member_t member;
        dc_guild_member_init(&member);
        benchmark::DoNotOptimize(member.has_user);
        dc_guild_member_free(&member);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Model_InitFree_GuildMember);

static void BM_JSON_Model_InitFree_Message(benchmark::State& state) {
    for (auto _ : state) {
        dc_message_t message;
        dc_message_init(&message);
        benchmark::DoNotOptimize(message.id);
        dc_message_free(&message);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_JSON_Model_InitFree_Message);

BENCHMARK_MAIN();
