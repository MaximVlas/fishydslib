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
