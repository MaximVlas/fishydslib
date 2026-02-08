/**
 * @file test_http.c
 * @brief HTTP tests (placeholder)
 */

#include "test_utils.h"
#include "http/dc_http.h"
#include "http/dc_http_compliance.h"
#include "http/dc_rest.h"
#include "core/dc_string.h"

typedef struct {
    int call_count;
} dc_rest_mock_state_t;

static void test_http_add_header(dc_http_response_t* resp, const char* name, const char* value) {
    dc_http_header_t h;
    dc_string_init_from_cstr(&h.name, name);
    dc_string_init_from_cstr(&h.value, value);
    dc_vec_push(&resp->headers, &h);
}

static dc_status_t test_rest_transport_rate_limit(void* userdata, const dc_http_request_t* request,
                                                  dc_http_response_t* response) {
    (void)request;
    dc_rest_mock_state_t* state = (dc_rest_mock_state_t*)userdata;
    state->call_count++;
    response->status_code = (state->call_count == 1) ? 429 : 200;
    dc_string_clear(&response->body);

    test_http_add_header(response, "X-RateLimit-Limit", "1");
    test_http_add_header(response, "X-RateLimit-Remaining", (state->call_count == 1) ? "0" : "1");
    test_http_add_header(response, "X-RateLimit-Reset-After", "0.001");
    test_http_add_header(response, "X-RateLimit-Bucket", "bucket-test");
    test_http_add_header(response, "Retry-After", "0.001");
    test_http_add_header(response, "X-RateLimit-Scope", "user");

    if (state->call_count == 1) {
        dc_string_set_cstr(&response->body,
                           "{\"message\":\"You are being rate limited.\",\"retry_after\":0.001,\"global\":false}");
    } else {
        dc_string_set_cstr(&response->body, "{\"ok\":true}");
    }
    return DC_OK;
}

static dc_status_t test_rest_transport_unauthorized(void* userdata, const dc_http_request_t* request,
                                                    dc_http_response_t* response) {
    (void)userdata;
    (void)request;
    response->status_code = 401;
    dc_string_clear(&response->body);
    dc_string_set_cstr(&response->body, "{\"message\":\"unauthorized\"}");
    return DC_OK;
}

int test_http_main(void) {
    TEST_SUITE_BEGIN("HTTP Tests");

    /* Base URL enforcement */
    dc_string_t url;
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&url), "init url string");
    TEST_ASSERT_EQ(DC_OK, dc_http_build_discord_api_url("/users/@me", &url), "build api url");
    TEST_ASSERT_STR_EQ("https://discord.com/api/v10/users/@me", dc_string_cstr(&url), "base url prefix");
    TEST_ASSERT_EQ(1, dc_http_is_discord_api_url(dc_string_cstr(&url)), "url validate ok");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_http_build_discord_api_url("https://example.com/api/v10", &url),
                   "reject non-discord base");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_http_build_discord_api_url("https://discordapp.com/api/v10", &url),
                   "reject legacy domain");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_http_build_discord_api_url("https://discord.com/api", &url),
                   "reject missing version");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_http_build_discord_api_url("https://discord.com/api/v9", &url),
                   "reject non-v10");
    dc_string_free(&url);

    /* User-Agent formatting */
    dc_user_agent_t ua = {
        .name = "fishydslib",
        .version = "0.1.0",
        .url = "https://example.com",
        .extra = "extra-info"
    };
    dc_string_t ua_str;
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&ua_str), "init ua string");
    TEST_ASSERT_EQ(DC_OK, dc_http_format_user_agent(&ua, &ua_str), "format user-agent");
    TEST_ASSERT_STR_EQ("DiscordBot (https://example.com, 0.1.0) fishydslib extra-info",
                       dc_string_cstr(&ua_str), "ua value");
    TEST_ASSERT_EQ(1, dc_http_user_agent_is_valid(dc_string_cstr(&ua_str)), "ua valid");
    TEST_ASSERT_EQ(0, dc_http_user_agent_is_valid("BadBot 1.0"), "ua invalid");
    TEST_ASSERT_EQ(0, dc_http_user_agent_is_valid("DiscordBot (https://example.com, 0.1.0)bad"),
                   "ua invalid suffix");
    TEST_ASSERT_EQ(1, dc_http_user_agent_is_valid("DiscordBot (https://example.com, 0.1.0) ok"),
                   "ua valid suffix");
    dc_string_free(&ua_str);

    /* Content-Type rules */
    TEST_ASSERT_EQ(1, dc_http_content_type_is_allowed("application/json"), "content-type json");
    TEST_ASSERT_EQ(1, dc_http_content_type_is_allowed("application/json; charset=utf-8"), "content-type json charset");
    TEST_ASSERT_EQ(1, dc_http_content_type_is_allowed("multipart/form-data; boundary=abc"), "content-type multipart");
    TEST_ASSERT_EQ(0, dc_http_content_type_is_allowed("text/plain"), "content-type reject");

    /* Authorization header formatting */
    dc_string_t auth;
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&auth), "init auth string");
    TEST_ASSERT_EQ(DC_OK, dc_http_format_auth_header(DC_HTTP_AUTH_BOT, "token123", &auth), "auth bot");
    TEST_ASSERT_STR_EQ("Bot token123", dc_string_cstr(&auth), "auth bot value");
    TEST_ASSERT_EQ(DC_OK, dc_http_format_auth_header(DC_HTTP_AUTH_BEARER, "token123", &auth), "auth bearer");
    TEST_ASSERT_STR_EQ("Bearer token123", dc_string_cstr(&auth), "auth bearer value");
    dc_string_free(&auth);

    /* Boolean query formatting */
    dc_string_t query;
    TEST_ASSERT_EQ(DC_OK, dc_string_init(&query), "init query");
    TEST_ASSERT_EQ(DC_OK, dc_http_append_query_bool(&query, "with_counts", 1, DC_HTTP_BOOL_TRUE_FALSE), "query bool tf");
    TEST_ASSERT_EQ(DC_OK, dc_http_append_query_bool(&query, "limit", 0, DC_HTTP_BOOL_ONE_ZERO), "query bool 10");
    TEST_ASSERT_STR_EQ("?with_counts=true&limit=0", dc_string_cstr(&query), "query value");
    dc_string_free(&query);

    /* Error parsing */
    const char* err_json = "{\"code\":50035,\"message\":\"Invalid Form Body\",\"errors\":{\"content\":{\"_errors\":[{\"code\":\"BASE_TYPE_REQUIRED\",\"message\":\"This field is required\"}]}}}";
    dc_http_error_t err;
    TEST_ASSERT_EQ(DC_OK, dc_http_error_init(&err), "init error");
    TEST_ASSERT_EQ(DC_OK, dc_http_error_parse(err_json, 0, &err), "parse error");
    TEST_ASSERT_EQ(50035, err.code, "error code");
    TEST_ASSERT_STR_EQ("Invalid Form Body", dc_string_cstr(&err.message), "error message");
    TEST_ASSERT_NEQ(0u, dc_string_length(&err.errors), "error errors json");
    dc_http_error_free(&err);

    /* JSON validation */
    TEST_ASSERT_EQ(DC_OK, dc_http_validate_json_body("{\"a\":1}", 0), "validate json ok");
    TEST_ASSERT_NEQ(DC_OK, dc_http_validate_json_body("{\"a\":", 0), "validate json invalid");

    /* Rate limit headers */
    dc_http_response_t resp;
    TEST_ASSERT_EQ(DC_OK, dc_http_response_init(&resp), "init response");
    {
        dc_http_header_t h1;
        dc_string_init_from_cstr(&h1.name, "X-RateLimit-Limit");
        dc_string_init_from_cstr(&h1.value, "5");
        dc_vec_push(&resp.headers, &h1);

        dc_http_header_t h2;
        dc_string_init_from_cstr(&h2.name, "X-RateLimit-Remaining");
        dc_string_init_from_cstr(&h2.value, "1");
        dc_vec_push(&resp.headers, &h2);

        dc_http_header_t h3;
        dc_string_init_from_cstr(&h3.name, "X-RateLimit-Bucket");
        dc_string_init_from_cstr(&h3.value, "abcd1234");
        dc_vec_push(&resp.headers, &h3);

        dc_http_header_t h4;
        dc_string_init_from_cstr(&h4.name, "X-RateLimit-Scope");
        dc_string_init_from_cstr(&h4.value, "shared");
        dc_vec_push(&resp.headers, &h4);

        dc_http_header_t h5;
        dc_string_init_from_cstr(&h5.name, "Retry-After");
        dc_string_init_from_cstr(&h5.value, "1.5");
        dc_vec_push(&resp.headers, &h5);
    }

    dc_http_rate_limit_t rl;
    TEST_ASSERT_EQ(DC_OK, dc_http_rate_limit_init(&rl), "init rate limit");
    TEST_ASSERT_EQ(DC_OK, dc_http_response_parse_rate_limit(&resp, &rl), "parse rate limit");
    TEST_ASSERT_EQ(5, rl.limit, "rl limit");
    TEST_ASSERT_EQ(1, rl.remaining, "rl remaining");
    TEST_ASSERT_EQ(DC_HTTP_RATE_LIMIT_SCOPE_SHARED, rl.scope, "rl scope");
    TEST_ASSERT_EQ(1.5, rl.retry_after, "rl retry-after");
    TEST_ASSERT_STR_EQ("abcd1234", dc_string_cstr(&rl.bucket), "rl bucket");
    dc_http_rate_limit_free(&rl);
    dc_http_response_free(&resp);

    /* 429 response JSON */
    const char* rl_json = "{\"message\":\"You are being rate limited.\",\"retry_after\":64.57,\"global\":false}";
    dc_http_rate_limit_response_t rlr;
    TEST_ASSERT_EQ(DC_OK, dc_http_rate_limit_response_init(&rlr), "init rl response");
    TEST_ASSERT_EQ(DC_OK, dc_http_rate_limit_response_parse(rl_json, 0, &rlr), "parse rl response");
    TEST_ASSERT_STR_EQ("You are being rate limited.", dc_string_cstr(&rlr.message), "rl message");
    TEST_ASSERT_EQ(0, rlr.global, "rl global");
    TEST_ASSERT_EQ(64.57, rlr.retry_after, "rl retry-after");
    dc_http_rate_limit_response_free(&rlr);

    /* REST rate limit retry */
    dc_rest_client_t* rest_client = NULL;
    dc_rest_mock_state_t mock_state = {0};
    dc_rest_client_config_t rest_cfg = {
        .token = "token123",
        .auth_type = DC_HTTP_AUTH_BOT,
        .user_agent = "DiscordBot (https://example.com, 0.1.0) fishydslib",
        .timeout_ms = 0,
        .max_retries = 2,
        .global_rate_limit_per_sec = 50,
        .global_window_ms = 1000,
        .invalid_request_limit = 10000,
        .invalid_request_window_ms = 600000,
        .transport = test_rest_transport_rate_limit,
        .transport_userdata = &mock_state
    };
    TEST_ASSERT_EQ(DC_OK, dc_rest_client_create(&rest_cfg, &rest_client), "rest client create");

    dc_rest_request_t rest_req;
    dc_rest_response_t rest_resp;
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_init(&rest_req), "rest request init");
    TEST_ASSERT_EQ(DC_OK, dc_rest_response_init(&rest_resp), "rest response init");
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_method(&rest_req, DC_HTTP_POST), "rest request method");
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_path(&rest_req, "/channels/123/messages"), "rest request path");
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_json_body(&rest_req, "{\"content\":\"hi\"}"), "rest json body");
    TEST_ASSERT_EQ(DC_OK, dc_rest_execute(rest_client, &rest_req, &rest_resp), "rest execute with retry");
    TEST_ASSERT_EQ(2, mock_state.call_count, "rest retry call count");
    TEST_ASSERT_EQ(200, rest_resp.http.status_code, "rest final status");
    dc_rest_response_free(&rest_resp);
    dc_rest_request_free(&rest_req);
    dc_rest_client_free(rest_client);

    /* REST invalid request tracking */
    dc_rest_client_t* invalid_client = NULL;
    dc_rest_client_config_t invalid_cfg = rest_cfg;
    invalid_cfg.transport = test_rest_transport_unauthorized;
    invalid_cfg.transport_userdata = NULL;
    invalid_cfg.invalid_request_limit = 2;
    invalid_cfg.max_retries = 0;
    TEST_ASSERT_EQ(DC_OK, dc_rest_client_create(&invalid_cfg, &invalid_client), "rest client invalid create");
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_init(&rest_req), "rest request init invalid");
    TEST_ASSERT_EQ(DC_OK, dc_rest_response_init(&rest_resp), "rest response init invalid");
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_path(&rest_req, "/users/@me"), "rest request path invalid");
    TEST_ASSERT_EQ(DC_ERROR_UNAUTHORIZED, dc_rest_execute(invalid_client, &rest_req, &rest_resp),
                   "rest unauthorized 1");
    TEST_ASSERT_EQ(DC_ERROR_UNAUTHORIZED, dc_rest_execute(invalid_client, &rest_req, &rest_resp),
                   "rest unauthorized 2");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_STATE, dc_rest_execute(invalid_client, &rest_req, &rest_resp),
                   "rest invalid limit reached");
    dc_rest_response_free(&rest_resp);
    dc_rest_request_free(&rest_req);
    dc_rest_client_free(invalid_client);
    
    TEST_SUITE_END("HTTP Tests");
}
