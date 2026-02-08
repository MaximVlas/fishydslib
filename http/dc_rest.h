#ifndef DC_REST_H
#define DC_REST_H

/**
 * @file dc_rest.h
 * @brief REST client with rate limit handling
 */

#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_vec.h"
#include "http/dc_http.h"
#include "http/dc_http_compliance.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dc_rest_client dc_rest_client_t;

typedef dc_status_t (*dc_rest_transport_fn)(void* userdata,
                                            const dc_http_request_t* request,
                                            dc_http_response_t* response);

typedef struct {
    const char* token;                  /**< Bot token */
    dc_http_auth_type_t auth_type;      /**< Bot/Bearer */
    const char* user_agent;             /**< Optional explicit User-Agent */
    uint32_t timeout_ms;                /**< Default request timeout */
    uint32_t max_retries;               /**< Retries for 429 responses (default 1) */
    uint32_t global_rate_limit_per_sec; /**< Global limit guard (default 50) */
    uint32_t global_window_ms;          /**< Global limit window (default 1000ms) */
    uint32_t invalid_request_limit;     /**< Invalid request threshold (default 10000) */
    uint32_t invalid_request_window_ms; /**< Invalid request window (default 600000ms) */
    dc_rest_transport_fn transport;     /**< Optional transport override */
    void* transport_userdata;           /**< Transport user data */
} dc_rest_client_config_t;

typedef struct {
    dc_http_method_t method;    /**< HTTP method */
    dc_string_t path;           /**< Path or full URL */
    dc_vec_t headers;           /**< dc_http_header_t */
    dc_string_t body;           /**< Request body */
    uint32_t timeout_ms;        /**< Optional timeout override */
    int body_is_json;           /**< If body is JSON */
    int is_interaction;         /**< Exempt from global rate limit guard */
} dc_rest_request_t;

typedef struct {
    dc_http_response_t http;                   /**< Raw HTTP response */
    dc_http_error_t error;                     /**< Parsed error JSON */
    dc_http_rate_limit_t rate_limit;           /**< Parsed rate limit headers */
    dc_http_rate_limit_response_t rate_limit_response; /**< Parsed 429 body */
} dc_rest_response_t;

dc_status_t dc_rest_client_create(const dc_rest_client_config_t* config, dc_rest_client_t** out_client);
void dc_rest_client_free(dc_rest_client_t* client);

dc_status_t dc_rest_request_init(dc_rest_request_t* request);
void dc_rest_request_free(dc_rest_request_t* request);
dc_status_t dc_rest_request_set_method(dc_rest_request_t* request, dc_http_method_t method);
dc_status_t dc_rest_request_set_path(dc_rest_request_t* request, const char* path);
dc_status_t dc_rest_request_add_header(dc_rest_request_t* request, const char* name, const char* value);
dc_status_t dc_rest_request_set_body(dc_rest_request_t* request, const char* body);
dc_status_t dc_rest_request_set_body_buffer(dc_rest_request_t* request, const void* body, size_t length);
dc_status_t dc_rest_request_set_json_body(dc_rest_request_t* request, const char* json_body);
dc_status_t dc_rest_request_set_timeout(dc_rest_request_t* request, uint32_t timeout_ms);
dc_status_t dc_rest_request_set_interaction(dc_rest_request_t* request, int is_interaction);

dc_status_t dc_rest_response_init(dc_rest_response_t* response);
void dc_rest_response_free(dc_rest_response_t* response);

dc_status_t dc_rest_execute(dc_rest_client_t* client, const dc_rest_request_t* request,
                            dc_rest_response_t* response);

#ifdef __cplusplus
}
#endif

#endif /* DC_REST_H */
