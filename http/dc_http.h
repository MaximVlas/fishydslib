#ifndef DC_HTTP_H
#define DC_HTTP_H

/**
 * @file dc_http.h
 * @brief HTTP client implementation using libcurl
 */

#include <stddef.h>
#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_vec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations from dc_http_compliance.h */
typedef struct dc_http_rate_limit dc_http_rate_limit_t;

/**
 * @brief HTTP methods
 */
typedef enum {
    DC_HTTP_GET,
    DC_HTTP_POST,
    DC_HTTP_PUT,
    DC_HTTP_PATCH,
    DC_HTTP_DELETE,
    DC_HTTP_HEAD,
    DC_HTTP_OPTIONS
} dc_http_method_t;

/**
 * @brief HTTP header structure
 */
typedef struct {
    dc_string_t name;   /**< Header name */
    dc_string_t value;  /**< Header value */
} dc_http_header_t;

/**
 * @brief HTTP request structure
 */
typedef struct {
    dc_http_method_t method;    /**< HTTP method */
    dc_string_t url;            /**< Request URL */
    dc_vec_t headers;           /**< Headers (dc_http_header_t) */
    dc_string_t body;           /**< Request body */
    uint32_t timeout_ms;        /**< Timeout in milliseconds */
} dc_http_request_t;

/**
 * @brief HTTP response structure
 */
typedef struct {
    long status_code;           /**< HTTP status code */
    dc_vec_t headers;           /**< Response headers (dc_http_header_t) */
    dc_string_t body;           /**< Response body */
    double total_time;          /**< Total request time in seconds */
} dc_http_response_t;

/**
 * @brief HTTP client structure
 */
typedef struct dc_http_client dc_http_client_t;

/**
 * @brief Create HTTP client
 * @param client Pointer to store created client
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_client_create(dc_http_client_t** client);

/**
 * @brief Free HTTP client
 * @param client Client to free
 */
void dc_http_client_free(dc_http_client_t* client);

/**
 * @brief Initialize HTTP request
 * @param request Request to initialize
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_request_init(dc_http_request_t* request);

/**
 * @brief Free HTTP request
 * @param request Request to free
 */
void dc_http_request_free(dc_http_request_t* request);

/**
 * @brief Initialize HTTP response
 * @param response Response to initialize
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_response_init(dc_http_response_t* response);

/**
 * @brief Free HTTP response
 * @param response Response to free
 */
void dc_http_response_free(dc_http_response_t* response);

/**
 * @brief Set request method
 * @param request Request to modify
 * @param method HTTP method
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_request_set_method(dc_http_request_t* request, dc_http_method_t method);

/**
 * @brief Set request URL
 * @param request Request to modify
 * @param url URL string
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_request_set_url(dc_http_request_t* request, const char* url);

/**
 * @brief Add request header
 * @param request Request to modify
 * @param name Header name
 * @param value Header value
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_request_add_header(dc_http_request_t* request, const char* name, const char* value);

/**
 * @brief Set request body
 * @param request Request to modify
 * @param body Body content
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_request_set_body(dc_http_request_t* request, const char* body);

/**
 * @brief Set request body from raw buffer
 * @param request Request to modify
 * @param body Body buffer (may contain null bytes)
 * @param length Buffer length
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_request_set_body_buffer(dc_http_request_t* request, const void* body, size_t length);

/**
 * @brief Set request body as JSON (validates JSON and sets Content-Type)
 * @param request Request to modify
 * @param json_body JSON body string
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_request_set_json_body(dc_http_request_t* request, const char* json_body);

/**
 * @brief Set request timeout
 * @param request Request to modify
 * @param timeout_ms Timeout in milliseconds
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_request_set_timeout(dc_http_request_t* request, uint32_t timeout_ms);

/**
 * @brief Execute HTTP request
 * @param client HTTP client
 * @param request Request to execute
 * @param response Response to store result
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_client_execute(dc_http_client_t* client, 
                                   const dc_http_request_t* request,
                                   dc_http_response_t* response);

/**
 * @brief Get response header value
 * @param response Response to search
 * @param name Header name
 * @param value Pointer to store header value
 * @return DC_OK if found, DC_ERROR_NOT_FOUND if not found, other error codes on failure
 */
dc_status_t dc_http_response_get_header(const dc_http_response_t* response, 
                                        const char* name, const char** value);

/**
 * @brief Parse rate limit headers from HTTP response
 * @param response Response to parse
 * @param rl Rate limit struct to populate
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_response_parse_rate_limit(const dc_http_response_t* response,
                                               dc_http_rate_limit_t* rl);

#ifdef __cplusplus
}
#endif

#endif /* DC_HTTP_H */
