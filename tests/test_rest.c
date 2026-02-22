/**
 * @file test_rest.c
 * @brief Tests for REST client with rate limiting
 */

#include "http/dc_rest.h"
#include "http/dc_http_compliance.h"
#include "core/dc_alloc.h"
#include "test_utils.h"
#include <string.h>
#include <stdio.h>

/* Mock transport for testing */
typedef struct {
    dc_http_response_t* mock_response;
    int call_count;
    dc_http_request_t last_request;
    int should_fail;
} mock_transport_ctx_t;

static dc_status_t mock_transport(void* userdata,
                                  const dc_http_request_t* request,
                                  dc_http_response_t* response) {
    mock_transport_ctx_t* ctx = (mock_transport_ctx_t*)userdata;
    if (!ctx || !request || !response) return DC_ERROR_NULL_POINTER;
    
    ctx->call_count++;
    
    /* Copy request for inspection */
    dc_http_request_free(&ctx->last_request);
    dc_http_request_init(&ctx->last_request);
    ctx->last_request.method = request->method;
    dc_string_set_cstr(&ctx->last_request.url, dc_string_cstr(&request->url));
    dc_string_set_cstr(&ctx->last_request.body, dc_string_cstr(&request->body));
    for (size_t i = 0; i < request->headers.length; i++) {
        dc_http_header_t* h = (dc_http_header_t*)dc_vec_at(&request->headers, i);
        if (!h) continue;
        dc_http_request_add_header(&ctx->last_request,
                                   dc_string_cstr(&h->name),
                                   dc_string_cstr(&h->value));
    }
    
    if (ctx->should_fail) {
        return DC_ERROR_NETWORK;
    }
    
    if (ctx->mock_response) {
        response->status_code = ctx->mock_response->status_code;
        dc_string_set_cstr(&response->body, dc_string_cstr(&ctx->mock_response->body));
        
        /* Copy headers */
        for (size_t i = 0; i < ctx->mock_response->headers.length; i++) {
            dc_http_header_t* h = (dc_http_header_t*)dc_vec_at(&ctx->mock_response->headers, i);
            if (h) {
                dc_http_header_t header;
                dc_string_init_from_cstr(&header.name, dc_string_cstr(&h->name));
                dc_string_init_from_cstr(&header.value, dc_string_cstr(&h->value));
                dc_vec_push(&response->headers, &header);
            }
        }
    }
    
    return DC_OK;
}

void test_rest_client_create(void) {
    dc_rest_client_config_t config = {
        .token = "test_token",
        .auth_type = DC_HTTP_AUTH_BOT,
        .user_agent = NULL,
        .timeout_ms = 5000,
        .max_retries = 3,
        .global_rate_limit_per_sec = 50,
        .global_window_ms = 1000,
        .invalid_request_limit = 10000,
        .invalid_request_window_ms = 600000,
        .transport = NULL,
        .transport_userdata = NULL
    };
    
    dc_rest_client_t* client = NULL;
    dc_status_t st = dc_rest_client_create(&config, &client);
    TEST_ASSERT(st == DC_OK, "Client creation should succeed");
    TEST_ASSERT(client != NULL, "Client should not be NULL");
    
    dc_rest_client_free(client);
}

void test_rest_client_create_invalid(void) {
    
    
    dc_rest_client_t* client = NULL;
    
    /* NULL config */
    dc_status_t st = dc_rest_client_create(NULL, &client);
    TEST_ASSERT(st == DC_ERROR_NULL_POINTER, "NULL config should fail");
    
    /* NULL output */
    dc_rest_client_config_t config = {
        .token = "test_token",
        .auth_type = DC_HTTP_AUTH_BOT
    };
    st = dc_rest_client_create(&config, NULL);
    TEST_ASSERT(st == DC_ERROR_NULL_POINTER, "NULL output should fail");
    
    /* Empty token */
    config.token = "";
    st = dc_rest_client_create(&config, &client);
    TEST_ASSERT(st == DC_ERROR_INVALID_PARAM, "Empty token should fail");
    
    /* NULL token */
    config.token = NULL;
    st = dc_rest_client_create(&config, &client);
    TEST_ASSERT(st == DC_ERROR_INVALID_PARAM, "NULL token should be invalid");
    
    
}

void test_rest_request_init(void) {
    
    
    dc_rest_request_t request;
    dc_status_t st = dc_rest_request_init(&request);
    TEST_ASSERT(st == DC_OK, "Request init should succeed");
    TEST_ASSERT(request.method == DC_HTTP_GET, "Default method should be GET");
    TEST_ASSERT(request.timeout_ms == 0, "Default timeout should be 0");
    TEST_ASSERT(request.body_is_json == 0, "body_is_json should be 0");
    TEST_ASSERT(request.is_interaction == 0, "is_interaction should be 0");
    
    dc_rest_request_free(&request);
    
    
}

void test_rest_request_set_path(void) {
    
    
    dc_rest_request_t request;
    dc_rest_request_init(&request);
    
    /* Relative path */
    dc_status_t st = dc_rest_request_set_path(&request, "/users/@me");
    TEST_ASSERT(st == DC_OK, "Setting relative path should succeed");
    TEST_ASSERT(strcmp(dc_string_cstr(&request.path), "/users/@me") == 0,
                "Path should be set correctly");
    
    /* Full URL */
    st = dc_rest_request_set_path(&request, "https://discord.com/api/v10/users/@me");
    TEST_ASSERT(st == DC_OK, "Setting full URL should succeed");
    
    dc_rest_request_free(&request);
    
    
}

void test_rest_request_set_json_body(void) {
    
    
    dc_rest_request_t request;
    dc_rest_request_init(&request);
    
    const char* json = "{\"content\":\"Hello\"}";
    dc_status_t st = dc_rest_request_set_json_body(&request, json);
    TEST_ASSERT(st == DC_OK, "Setting JSON body should succeed");
    TEST_ASSERT(request.body_is_json == 1, "body_is_json should be set");
    TEST_ASSERT(strcmp(dc_string_cstr(&request.body), json) == 0,
                "Body should be set correctly");
    
    /* Check Content-Type header was added */
    int found_ct = 0;
    for (size_t i = 0; i < request.headers.length; i++) {
        dc_http_header_t* h = (dc_http_header_t*)dc_vec_at(&request.headers, i);
        if (h && strcmp(dc_string_cstr(&h->name), "Content-Type") == 0) {
            found_ct = 1;
            TEST_ASSERT(strcmp(dc_string_cstr(&h->value), "application/json") == 0,
                        "Content-Type should be application/json");
        }
    }
    TEST_ASSERT(found_ct, "Content-Type header should be added");
    
    dc_rest_request_free(&request);
    
    
}

void test_rest_request_headers(void) {
    
    
    dc_rest_request_t request;
    dc_rest_request_init(&request);
    
    /* Add custom header */
    dc_status_t st = dc_rest_request_add_header(&request, "X-Custom", "value");
    TEST_ASSERT(st == DC_OK, "Adding custom header should succeed");
    
    /* Cannot override Authorization */
    st = dc_rest_request_add_header(&request, "Authorization", "Bearer token");
    TEST_ASSERT(st == DC_ERROR_INVALID_PARAM, "Cannot override Authorization");
    
    /* Cannot override User-Agent */
    st = dc_rest_request_add_header(&request, "User-Agent", "custom");
    TEST_ASSERT(st == DC_ERROR_INVALID_PARAM, "Cannot override User-Agent");
    
    dc_rest_request_free(&request);
    
    
}

void test_rest_execute_basic(void) {
    
    
    /* Setup mock transport */
    mock_transport_ctx_t mock_ctx = {0};
    dc_http_response_t mock_response;
    dc_http_response_init(&mock_response);
    mock_response.status_code = 200;
    dc_string_set_cstr(&mock_response.body, "{\"id\":\"123\"}");
    mock_ctx.mock_response = &mock_response;
    dc_http_request_init(&mock_ctx.last_request);
    
    dc_rest_client_config_t config = {
        .token = "test_token",
        .auth_type = DC_HTTP_AUTH_BOT,
        .transport = mock_transport,
        .transport_userdata = &mock_ctx
    };
    
    dc_rest_client_t* client = NULL;
    dc_status_t st = dc_rest_client_create(&config, &client);
    TEST_ASSERT(st == DC_OK, "Client creation should succeed");
    
    dc_rest_request_t request;
    dc_rest_request_init(&request);
    dc_rest_request_set_method(&request, DC_HTTP_GET);
    dc_rest_request_set_path(&request, "/users/@me");
    
    dc_rest_response_t response;
    dc_rest_response_init(&response);
    
    st = dc_rest_execute(client, &request, &response);
    TEST_ASSERT(st == DC_OK, "Execute should succeed");
    TEST_ASSERT(mock_ctx.call_count == 1, "Transport should be called once");
    TEST_ASSERT(response.http.status_code == 200, "Status should be 200");
    TEST_ASSERT(strcmp(dc_string_cstr(&response.http.body), "{\"id\":\"123\"}") == 0,
                "Body should match");
    
    dc_rest_response_free(&response);
    dc_rest_request_free(&request);
    dc_http_request_free(&mock_ctx.last_request);
    dc_http_response_free(&mock_response);
    dc_rest_client_free(client);
    
    
}

void test_rest_execute_with_rate_limit_headers(void) {
    
    
    /* Setup mock transport with rate limit headers */
    mock_transport_ctx_t mock_ctx = {0};
    dc_http_response_t mock_response;
    dc_http_response_init(&mock_response);
    mock_response.status_code = 200;
    dc_string_set_cstr(&mock_response.body, "{}");
    
    /* Add rate limit headers */
    dc_http_header_t h1, h2, h3, h4;
    dc_string_init_from_cstr(&h1.name, "X-RateLimit-Limit");
    dc_string_init_from_cstr(&h1.value, "10");
    dc_vec_push(&mock_response.headers, &h1);
    
    dc_string_init_from_cstr(&h2.name, "X-RateLimit-Remaining");
    dc_string_init_from_cstr(&h2.value, "9");
    dc_vec_push(&mock_response.headers, &h2);
    
    dc_string_init_from_cstr(&h3.name, "X-RateLimit-Reset");
    dc_string_init_from_cstr(&h3.value, "1234567890.5");
    dc_vec_push(&mock_response.headers, &h3);
    
    dc_string_init_from_cstr(&h4.name, "X-RateLimit-Bucket");
    dc_string_init_from_cstr(&h4.value, "test-bucket-id");
    dc_vec_push(&mock_response.headers, &h4);
    
    mock_ctx.mock_response = &mock_response;
    dc_http_request_init(&mock_ctx.last_request);
    
    dc_rest_client_config_t config = {
        .token = "test_token",
        .auth_type = DC_HTTP_AUTH_BOT,
        .transport = mock_transport,
        .transport_userdata = &mock_ctx
    };
    
    dc_rest_client_t* client = NULL;
    dc_rest_client_create(&config, &client);
    
    dc_rest_request_t request;
    dc_rest_request_init(&request);
    dc_rest_request_set_path(&request, "/channels/123/messages");
    
    dc_rest_response_t response;
    dc_rest_response_init(&response);
    
    dc_status_t st = dc_rest_execute(client, &request, &response);
    TEST_ASSERT(st == DC_OK, "Execute should succeed");
    TEST_ASSERT(response.rate_limit.limit == 10, "Rate limit should be parsed");
    TEST_ASSERT(response.rate_limit.remaining == 9, "Remaining should be parsed");
    TEST_ASSERT(strcmp(dc_string_cstr(&response.rate_limit.bucket), "test-bucket-id") == 0,
                "Bucket ID should be parsed");
    
    dc_rest_response_free(&response);
    dc_rest_request_free(&request);
    dc_http_request_free(&mock_ctx.last_request);
    dc_http_response_free(&mock_response);
    dc_rest_client_free(client);
    
    
}

void test_rest_execute_429_retry(void) {
    
    
    /* Setup mock transport that returns 429 then 200 */
    mock_transport_ctx_t mock_ctx = {0};
    dc_http_response_t mock_response;
    dc_http_response_init(&mock_response);
    dc_http_request_init(&mock_ctx.last_request);
    
    dc_rest_client_config_t config = {
        .token = "test_token",
        .auth_type = DC_HTTP_AUTH_BOT,
        .max_retries = 2,
        .transport = mock_transport,
        .transport_userdata = &mock_ctx
    };
    
    dc_rest_client_t* client = NULL;
    dc_rest_client_create(&config, &client);
    
    dc_rest_request_t request;
    dc_rest_request_init(&request);
    dc_rest_request_set_path(&request, "/channels/123/messages");
    
    dc_rest_response_t response;
    dc_rest_response_init(&response);
    
    /* First call returns 429 */
    mock_response.status_code = 429;
    dc_string_set_cstr(&mock_response.body, 
                       "{\"message\":\"Rate limited\",\"retry_after\":0.01,\"global\":false}");
    dc_http_header_t h1;
    dc_string_init_from_cstr(&h1.name, "Retry-After");
    dc_string_init_from_cstr(&h1.value, "0.01");
    dc_vec_push(&mock_response.headers, &h1);
    mock_ctx.mock_response = &mock_response;
    
    dc_status_t st = dc_rest_execute(client, &request, &response);
    
    /* Should retry and eventually return 429 status */
    TEST_ASSERT(st == DC_ERROR_RATE_LIMITED, "Should return rate limited status");
    TEST_ASSERT(mock_ctx.call_count >= 1, "Should attempt at least once");
    
    dc_rest_response_free(&response);
    dc_rest_request_free(&request);
    dc_http_request_free(&mock_ctx.last_request);
    dc_http_response_free(&mock_response);
    dc_rest_client_free(client);
    
    
}

void test_rest_execute_error_parsing(void) {
    
    
    /* Setup mock transport with error response */
    mock_transport_ctx_t mock_ctx = {0};
    dc_http_response_t mock_response;
    dc_http_response_init(&mock_response);
    mock_response.status_code = 400;
    dc_string_set_cstr(&mock_response.body,
                       "{\"code\":50035,\"message\":\"Invalid Form Body\"}");
    mock_ctx.mock_response = &mock_response;
    dc_http_request_init(&mock_ctx.last_request);
    
    dc_rest_client_config_t config = {
        .token = "test_token",
        .auth_type = DC_HTTP_AUTH_BOT,
        .transport = mock_transport,
        .transport_userdata = &mock_ctx
    };
    
    dc_rest_client_t* client = NULL;
    dc_rest_client_create(&config, &client);
    
    dc_rest_request_t request;
    dc_rest_request_init(&request);
    dc_rest_request_set_path(&request, "/channels/123/messages");
    dc_rest_request_set_json_body(&request, "{\"content\":\"\"}");
    dc_rest_request_set_method(&request, DC_HTTP_POST);
    
    dc_rest_response_t response;
    dc_rest_response_init(&response);
    
    dc_status_t st = dc_rest_execute(client, &request, &response);
    TEST_ASSERT(st == DC_ERROR_BAD_REQUEST, "Should return bad request status");
    TEST_ASSERT(response.error.code == 50035, "Error code should be parsed");
    TEST_ASSERT(strcmp(dc_string_cstr(&response.error.message), "Invalid Form Body") == 0,
                "Error message should be parsed");
    
    dc_rest_response_free(&response);
    dc_rest_request_free(&request);
    dc_http_request_free(&mock_ctx.last_request);
    dc_http_response_free(&mock_response);
    dc_rest_client_free(client);
    
    
}

void test_rest_interaction_endpoint_exemption(void) {
    
    
    /* Interaction endpoints should be exempt from global rate limit */
    mock_transport_ctx_t mock_ctx = {0};
    dc_http_response_t mock_response;
    dc_http_response_init(&mock_response);
    mock_response.status_code = 200;
    dc_string_set_cstr(&mock_response.body, "{}");
    mock_ctx.mock_response = &mock_response;
    dc_http_request_init(&mock_ctx.last_request);
    
    dc_rest_client_config_t config = {
        .token = "test_token",
        .auth_type = DC_HTTP_AUTH_BOT,
        .global_rate_limit_per_sec = 1, /* Very low limit */
        .transport = mock_transport,
        .transport_userdata = &mock_ctx
    };
    
    dc_rest_client_t* client = NULL;
    dc_rest_client_create(&config, &client);
    
    dc_rest_request_t request;
    dc_rest_request_init(&request);
    dc_rest_request_set_path(&request, "/interactions/123/token/callback");
    dc_rest_request_set_interaction(&request, 1);
    
    dc_rest_response_t response;
    dc_rest_response_init(&response);
    
    /* Should succeed even with low global limit */
    dc_status_t st = dc_rest_execute(client, &request, &response);
    TEST_ASSERT(st == DC_OK, "Interaction endpoint should succeed");
    
    dc_rest_response_free(&response);
    dc_rest_request_free(&request);
    dc_http_request_free(&mock_ctx.last_request);
    dc_http_response_free(&mock_response);
    dc_rest_client_free(client);
    
    
}

void test_rest_auth_header_injection(void) {
    
    
    /* Verify Authorization header is added automatically */
    mock_transport_ctx_t mock_ctx = {0};
    dc_http_response_t mock_response;
    dc_http_response_init(&mock_response);
    mock_response.status_code = 200;
    dc_string_set_cstr(&mock_response.body, "{}");
    mock_ctx.mock_response = &mock_response;
    dc_http_request_init(&mock_ctx.last_request);
    
    dc_rest_client_config_t config = {
        .token = "my_bot_token",
        .auth_type = DC_HTTP_AUTH_BOT,
        .transport = mock_transport,
        .transport_userdata = &mock_ctx
    };
    
    dc_rest_client_t* client = NULL;
    dc_rest_client_create(&config, &client);
    
    dc_rest_request_t request;
    dc_rest_request_init(&request);
    dc_rest_request_set_path(&request, "/users/@me");
    
    dc_rest_response_t response;
    dc_rest_response_init(&response);
    
    dc_rest_execute(client, &request, &response);
    
    /* Check that Authorization header was added to the HTTP request */
    int found_auth = 0;
    for (size_t i = 0; i < mock_ctx.last_request.headers.length; i++) {
        dc_http_header_t* h = (dc_http_header_t*)dc_vec_at(&mock_ctx.last_request.headers, i);
        if (h && strcmp(dc_string_cstr(&h->name), "Authorization") == 0) {
            found_auth = 1;
            TEST_ASSERT(strncmp(dc_string_cstr(&h->value), "Bot ", 4) == 0,
                        "Authorization should start with 'Bot '");
        }
    }
    TEST_ASSERT(found_auth, "Authorization header should be present");
    
    dc_rest_response_free(&response);
    dc_rest_request_free(&request);
    dc_http_request_free(&mock_ctx.last_request);
    dc_http_response_free(&mock_response);
    dc_rest_client_free(client);
    
    
}

void test_rest_execute_documented_routes(void) {
    mock_transport_ctx_t mock_ctx = {0};
    dc_http_response_t mock_response;
    dc_http_response_init(&mock_response);
    mock_response.status_code = 200;
    dc_string_set_cstr(&mock_response.body, "{}");
    mock_ctx.mock_response = &mock_response;
    dc_http_request_init(&mock_ctx.last_request);

    dc_rest_client_config_t config = {
        .token = "test_token",
        .auth_type = DC_HTTP_AUTH_BOT,
        .transport = mock_transport,
        .transport_userdata = &mock_ctx
    };

    dc_rest_client_t* client = NULL;
    TEST_ASSERT_EQ(DC_OK, dc_rest_client_create(&config, &client),
                   "create client for documented routes");

    typedef struct {
        dc_http_method_t method;
        const char* path;
        const char* json_body;
    } route_case_t;

    const route_case_t cases[] = {
        {DC_HTTP_POST, "/stage-instances", "{\"channel_id\":\"123\",\"topic\":\"t\"}"},
        {DC_HTTP_PATCH, "/stage-instances/123", "{\"topic\":\"updated\"}"},
        {DC_HTTP_DELETE, "/stage-instances/123", NULL},
        {DC_HTTP_GET, "/channels/111/polls/222/answers/1?limit=25&after=333", NULL},
        {DC_HTTP_POST, "/channels/111/polls/222/expire", NULL},
        {DC_HTTP_POST, "/channels/111/send-soundboard-sound", "{\"sound_id\":\"444\"}"},
        {DC_HTTP_GET, "/soundboard-default-sounds", NULL},
        {DC_HTTP_PATCH, "/guilds/999/voice-states/@me", "{\"suppress\":true}"},
        {DC_HTTP_PATCH, "/guilds/999/voice-states/123", "{\"suppress\":false}"},
        {DC_HTTP_GET, "https://discord.com/api/v10/voice/regions", NULL}
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); ++i) {
        dc_rest_request_t request;
        dc_rest_response_t response;
        TEST_ASSERT_EQ(DC_OK, dc_rest_request_init(&request), "init route request");
        TEST_ASSERT_EQ(DC_OK, dc_rest_response_init(&response), "init route response");

        TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_method(&request, cases[i].method),
                       "set documented route method");
        TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_path(&request, cases[i].path),
                       "set documented route path");
        if (cases[i].json_body) {
            TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_json_body(&request, cases[i].json_body),
                           "set documented route json body");
        }

        TEST_ASSERT_EQ(DC_OK, dc_rest_execute(client, &request, &response),
                       "execute documented route");
        TEST_ASSERT_EQ((int)(i + 1), mock_ctx.call_count, "mock call count increments");
        TEST_ASSERT_EQ(cases[i].method, mock_ctx.last_request.method, "http method matches");
        TEST_ASSERT_NEQ(NULL, strstr(dc_string_cstr(&mock_ctx.last_request.url), cases[i].path[0] == '/' ? cases[i].path : "/voice/regions"),
                        "request URL contains documented path");

        dc_rest_response_free(&response);
        dc_rest_request_free(&request);
    }

    dc_http_request_free(&mock_ctx.last_request);
    dc_http_response_free(&mock_response);
    dc_rest_client_free(client);
}

void test_rest_execute_rejects_non_https_full_url(void) {
    mock_transport_ctx_t mock_ctx = {0};
    dc_http_response_t mock_response;
    dc_http_response_init(&mock_response);
    mock_response.status_code = 200;
    dc_string_set_cstr(&mock_response.body, "{}");
    mock_ctx.mock_response = &mock_response;
    dc_http_request_init(&mock_ctx.last_request);

    dc_rest_client_config_t config = {
        .token = "test_token",
        .auth_type = DC_HTTP_AUTH_BOT,
        .transport = mock_transport,
        .transport_userdata = &mock_ctx
    };

    dc_rest_client_t* client = NULL;
    TEST_ASSERT_EQ(DC_OK, dc_rest_client_create(&config, &client),
                   "create client for non-https validation");

    dc_rest_request_t request;
    dc_rest_response_t response;
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_init(&request), "init non-https request");
    TEST_ASSERT_EQ(DC_OK, dc_rest_response_init(&response), "init non-https response");
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_method(&request, DC_HTTP_GET), "set non-https method");
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_path(&request, "http://discord.com/api/v10/users/@me"),
                   "set non-https path");

    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_rest_execute(client, &request, &response),
                   "reject non-https full url");
    TEST_ASSERT_EQ(0, mock_ctx.call_count, "transport not called for invalid full url");

    dc_rest_response_free(&response);
    dc_rest_request_free(&request);
    dc_http_request_free(&mock_ctx.last_request);
    dc_http_response_free(&mock_response);
    dc_rest_client_free(client);
}

void test_rest_execute_rejects_non_discord_https_full_url(void) {
    mock_transport_ctx_t mock_ctx = {0};
    dc_http_response_t mock_response;
    dc_http_response_init(&mock_response);
    mock_response.status_code = 200;
    dc_string_set_cstr(&mock_response.body, "{}");
    mock_ctx.mock_response = &mock_response;
    dc_http_request_init(&mock_ctx.last_request);

    dc_rest_client_config_t config = {
        .token = "test_token",
        .auth_type = DC_HTTP_AUTH_BOT,
        .transport = mock_transport,
        .transport_userdata = &mock_ctx
    };

    dc_rest_client_t* client = NULL;
    TEST_ASSERT_EQ(DC_OK, dc_rest_client_create(&config, &client),
                   "create client for full-url host/version validation");

    const char* invalid_urls[] = {
        "https://example.com/api/v10/users/@me",
        "https://discordapp.com/api/v10/users/@me",
        "https://discord.com/api/v9/users/@me"
    };

    for (size_t i = 0; i < (sizeof(invalid_urls) / sizeof(invalid_urls[0])); ++i) {
        dc_rest_request_t request;
        dc_rest_response_t response;
        TEST_ASSERT_EQ(DC_OK, dc_rest_request_init(&request), "init invalid-host request");
        TEST_ASSERT_EQ(DC_OK, dc_rest_response_init(&response), "init invalid-host response");
        TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_method(&request, DC_HTTP_GET), "set invalid-host method");
        TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_path(&request, invalid_urls[i]), "set invalid-host path");

        TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_rest_execute(client, &request, &response),
                       "reject non-discord or non-v10 full url");
        TEST_ASSERT_EQ(0, mock_ctx.call_count, "transport not called for invalid full url");

        dc_rest_response_free(&response);
        dc_rest_request_free(&request);
    }

    dc_http_request_free(&mock_ctx.last_request);
    dc_http_response_free(&mock_response);
    dc_rest_client_free(client);
}

void test_rest_execute_requires_content_type_for_raw_body(void) {
    mock_transport_ctx_t mock_ctx = {0};
    dc_http_response_t mock_response;
    dc_http_response_init(&mock_response);
    mock_response.status_code = 200;
    dc_string_set_cstr(&mock_response.body, "{}");
    mock_ctx.mock_response = &mock_response;
    dc_http_request_init(&mock_ctx.last_request);

    dc_rest_client_config_t config = {
        .token = "test_token",
        .auth_type = DC_HTTP_AUTH_BOT,
        .transport = mock_transport,
        .transport_userdata = &mock_ctx
    };

    dc_rest_client_t* client = NULL;
    TEST_ASSERT_EQ(DC_OK, dc_rest_client_create(&config, &client),
                   "create client for raw body content-type checks");

    dc_rest_request_t request;
    dc_rest_response_t response;
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_init(&request), "init raw-body request");
    TEST_ASSERT_EQ(DC_OK, dc_rest_response_init(&response), "init raw-body response");
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_method(&request, DC_HTTP_POST), "set raw-body method");
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_set_path(&request, "/channels/123/messages"), "set raw-body path");
    TEST_ASSERT_EQ(DC_OK, dc_string_set_cstr(&request.body, "payload=1"), "set raw-body payload");

    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM, dc_rest_execute(client, &request, &response),
                   "raw body without content-type rejected");
    TEST_ASSERT_EQ(0, mock_ctx.call_count, "transport not called without content-type");

    TEST_ASSERT_EQ(DC_OK,
                   dc_rest_request_add_header(&request, "Content-Type",
                                              "application/x-www-form-urlencoded"),
                   "set form content-type");
    TEST_ASSERT_EQ(DC_OK, dc_rest_execute(client, &request, &response),
                   "raw body with content-type succeeds");
    TEST_ASSERT_EQ(1, mock_ctx.call_count, "transport called once after valid content-type");
    TEST_ASSERT_STR_EQ("payload=1", dc_string_cstr(&mock_ctx.last_request.body),
                       "raw body forwarded");

    dc_rest_response_free(&response);
    dc_rest_request_free(&request);
    dc_http_request_free(&mock_ctx.last_request);
    dc_http_response_free(&mock_response);
    dc_rest_client_free(client);
}

void test_rest_request_headers_case_insensitive_reserved(void) {
    dc_rest_request_t request;
    TEST_ASSERT_EQ(DC_OK, dc_rest_request_init(&request), "init case-insensitive header request");

    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_rest_request_add_header(&request, "authorization", "Bot token"),
                   "lowercase authorization is reserved");
    TEST_ASSERT_EQ(DC_ERROR_INVALID_PARAM,
                   dc_rest_request_add_header(&request, "user-agent", "bad"),
                   "lowercase user-agent is reserved");

    dc_rest_request_free(&request);
}
