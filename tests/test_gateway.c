/**
 * @file test_gateway.c
 * @brief Gateway tests
 */

#include "test_utils.h"
#include "gw/dc_gateway.h"
#include "core/dc_status.h"
#include <string.h>

static dc_gateway_config_t test_gateway_default_config(void) {
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

void test_gateway_close_code_strings(void) {
    TEST_ASSERT_STR_EQ("Authentication failed",
                       dc_gateway_close_code_string(DC_GATEWAY_CLOSE_AUTHENTICATION_FAILED),
                       "close code string auth failed");
    TEST_ASSERT_STR_EQ("Invalid intent(s)",
                       dc_gateway_close_code_string(DC_GATEWAY_CLOSE_INVALID_INTENTS),
                       "close code string invalid intents");
    TEST_ASSERT_STR_EQ("Unknown close code",
                       dc_gateway_close_code_string(9999),
                       "close code string unknown");
}

void test_gateway_close_code_reconnect(void) {
    TEST_ASSERT_EQ(0, dc_gateway_close_code_should_reconnect(DC_GATEWAY_CLOSE_AUTHENTICATION_FAILED),
                   "close code auth failed no reconnect");
    TEST_ASSERT_EQ(0, dc_gateway_close_code_should_reconnect(DC_GATEWAY_CLOSE_INVALID_SHARD),
                   "close code invalid shard no reconnect");
    TEST_ASSERT_EQ(0, dc_gateway_close_code_should_reconnect(DC_GATEWAY_CLOSE_INVALID_INTENTS),
                   "close code invalid intents no reconnect");
    TEST_ASSERT_EQ(1, dc_gateway_close_code_should_reconnect(DC_GATEWAY_CLOSE_UNKNOWN_ERROR),
                   "close code unknown error reconnect");
    TEST_ASSERT_EQ(1, dc_gateway_close_code_should_reconnect(1000),
                   "close code 1000 reconnect");
}

void test_gateway_client_create_invalid(void) {
    dc_gateway_client_t* client = NULL;
    dc_gateway_config_t cfg = test_gateway_default_config();

    TEST_ASSERT_EQ(DC_ERROR_NULL_POINTER, dc_gateway_client_create(NULL, &client),
                   "create null config");
    TEST_ASSERT_EQ(DC_ERROR_NULL_POINTER, dc_gateway_client_create(&cfg, NULL),
                   "create null out ptr");

    cfg.token = "";
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_gateway_client_create(&cfg, &client),
                   "create empty token");

    cfg = test_gateway_default_config();
    cfg.shard_id = 1;
    cfg.shard_count = 0;
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_gateway_client_create(&cfg, &client),
                   "create shard id without count");

    cfg = test_gateway_default_config();
    cfg.shard_id = 2;
    cfg.shard_count = 2;
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_gateway_client_create(&cfg, &client),
                   "create shard id out of range");

    cfg = test_gateway_default_config();
    cfg.large_threshold = 10;
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_gateway_client_create(&cfg, &client),
                   "create large_threshold too small");

    cfg = test_gateway_default_config();
    cfg.large_threshold = 300;
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_gateway_client_create(&cfg, &client),
                   "create large_threshold too large");

    cfg = test_gateway_default_config();
    cfg.enable_compression = 1;
    cfg.enable_payload_compression = 1;
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_gateway_client_create(&cfg, &client),
                   "create compression conflict");

    cfg = test_gateway_default_config();
    cfg.user_agent = "BadBot 1.0";
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_gateway_client_create(&cfg, &client),
                   "create invalid user agent");
}

void test_gateway_client_create_success(void) {
    dc_gateway_client_t* client = NULL;
    dc_gateway_config_t cfg = test_gateway_default_config();

    TEST_ASSERT_EQ(DC_OK, dc_gateway_client_create(&cfg, &client), "create success");
    TEST_ASSERT_NOT_NULL(client, "client not null");

    dc_gateway_state_t state = DC_GATEWAY_RECONNECTING;
    TEST_ASSERT_EQ(DC_OK, dc_gateway_client_get_state(client, &state), "get state ok");
    TEST_ASSERT_EQ(DC_GATEWAY_DISCONNECTED, state, "initial state disconnected");

    TEST_ASSERT_EQ(DC_OK, dc_gateway_client_disconnect(client), "disconnect ok");
    dc_gateway_client_free(client);
}

void test_gateway_update_presence_invalid(void) {
    dc_gateway_client_t* client = NULL;
    dc_gateway_config_t cfg = test_gateway_default_config();

    TEST_ASSERT_EQ(DC_OK, dc_gateway_client_create(&cfg, &client), "create for presence");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_STATE,
                   dc_gateway_client_update_presence(client, "online", NULL, 0),
                   "presence invalid state");
    TEST_ASSERT_EQ(DC_ERROR_NULL_POINTER,
                   dc_gateway_client_update_presence(NULL, "online", NULL, 0),
                   "presence null client");
    TEST_ASSERT_EQ(DC_ERROR_NULL_POINTER,
                   dc_gateway_client_update_presence(client, NULL, NULL, 0),
                   "presence null status");

    dc_gateway_client_free(client);
}

void test_gateway_client_connect_invalid(void) {
    dc_gateway_client_t* client = NULL;
    dc_gateway_config_t cfg = test_gateway_default_config();

    TEST_ASSERT_EQ(DC_OK, dc_gateway_client_create(&cfg, &client), "create for connect");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_gateway_client_connect(client, NULL),
                   "connect without url or cache");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_gateway_client_connect(client, ""),
                   "connect with empty url");

    dc_gateway_client_free(client);
}

void test_gateway_client_process_invalid(void) {
    dc_gateway_client_t* client = NULL;
    dc_gateway_config_t cfg = test_gateway_default_config();

    TEST_ASSERT_EQ(DC_OK, dc_gateway_client_create(&cfg, &client), "create for process");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_STATE, dc_gateway_client_process(client, 0),
                   "process without context");
    TEST_ASSERT_EQ(DC_ERROR_NULL_POINTER, dc_gateway_client_process(NULL, 0),
                   "process null client");

    dc_gateway_client_free(client);
}

void test_gateway_request_guild_members_invalid(void) {
    dc_gateway_client_t* client = NULL;
    dc_gateway_config_t cfg = test_gateway_default_config();
    const dc_snowflake_t guild_id = 123ULL;
    dc_snowflake_t ids[1] = { 123ULL };

    TEST_ASSERT_EQ(DC_OK, dc_gateway_client_create(&cfg, &client), "create for request guild members");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_STATE,
                   dc_gateway_client_request_guild_members(client, guild_id, "", 0, 0, NULL, (size_t)0, NULL),
                   "request guild members invalid state");
    TEST_ASSERT_EQ(DC_ERROR_NULL_POINTER,
                   dc_gateway_client_request_guild_members(NULL, guild_id, "", 0, 0, NULL, (size_t)0, NULL),
                   "request guild members null client");

    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_gateway_client_request_guild_members(client, DC_SNOWFLAKE_NULL, "", 0, 0, NULL, (size_t)0, NULL),
                   "request guild members invalid guild id");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_gateway_client_request_guild_members(client, guild_id, NULL, 0, 0, NULL, (size_t)0, NULL),
                   "request guild members missing query and user ids");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_gateway_client_request_guild_members(client, guild_id, "", 0, 0, ids, (size_t)1, NULL),
                   "request guild members query and user ids together");

    dc_gateway_client_free(client);
}

void test_gateway_request_soundboard_invalid(void) {
    dc_gateway_client_t* client = NULL;
    dc_gateway_config_t cfg = test_gateway_default_config();
    dc_snowflake_t guild_ids[1] = { 123ULL };

    TEST_ASSERT_EQ(DC_OK, dc_gateway_client_create(&cfg, &client), "create for request soundboard");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_STATE,
                   dc_gateway_client_request_soundboard_sounds(client, guild_ids, (size_t)1),
                   "request soundboard invalid state");
    TEST_ASSERT_EQ(DC_ERROR_NULL_POINTER,
                   dc_gateway_client_request_soundboard_sounds(NULL, guild_ids, (size_t)1),
                   "request soundboard null client");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_gateway_client_request_soundboard_sounds(client, NULL, (size_t)1),
                   "request soundboard null ids");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_gateway_client_request_soundboard_sounds(client, guild_ids, (size_t)0),
                   "request soundboard empty ids");

    dc_gateway_client_free(client);
}

void test_gateway_update_voice_state_invalid(void) {
    dc_gateway_client_t* client = NULL;
    dc_gateway_config_t cfg = test_gateway_default_config();
    const dc_snowflake_t guild_id = 123ULL;

    TEST_ASSERT_EQ(DC_OK, dc_gateway_client_create(&cfg, &client), "create for voice state");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_STATE,
                   dc_gateway_client_update_voice_state(client, guild_id, DC_SNOWFLAKE_NULL, 0, 0),
                   "voice state invalid state");
    TEST_ASSERT_EQ(DC_ERROR_NULL_POINTER,
                   dc_gateway_client_update_voice_state(NULL, guild_id, DC_SNOWFLAKE_NULL, 0, 0),
                   "voice state null client");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_gateway_client_update_voice_state(client, DC_SNOWFLAKE_NULL, DC_SNOWFLAKE_NULL, 0, 0),
                   "voice state invalid guild");

    dc_gateway_client_free(client);
}
