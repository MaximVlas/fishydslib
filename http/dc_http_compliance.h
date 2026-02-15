#ifndef DC_HTTP_COMPLIANCE_H
#define DC_HTTP_COMPLIANCE_H

/**
 * @file dc_http_compliance.h
 * @brief HTTP compliance helpers for Discord API
 */

#include <stddef.h>
#include "core/dc_status.h"
#include "core/dc_string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Discord API base URL (explicit v10)
 */
#define DC_DISCORD_API_BASE_URL "https://discord.com/api/v10"

/**
 * @brief Library identity for User-Agent
 */
#ifndef DC_HTTP_LIBRARY_NAME
#define DC_HTTP_LIBRARY_NAME "fishydslib"
#endif
#ifndef DC_HTTP_LIBRARY_VERSION
#define DC_HTTP_LIBRARY_VERSION "0.1.0"
#endif
#ifndef DC_HTTP_LIBRARY_URL
#define DC_HTTP_LIBRARY_URL "https://github.com"
#endif

/**
 * @brief User-Agent description
 */
typedef struct {
    const char* name;     /**< Optional suffix metadata (e.g., library name) */
    const char* version;  /**< Version string (required) */
    const char* url;      /**< Project URL (required) */
    const char* extra;    /**< Optional additional suffix metadata */
} dc_user_agent_t;

/**
 * @brief HTTP auth header type
 */
typedef enum {
    DC_HTTP_AUTH_BOT,
    DC_HTTP_AUTH_BEARER
} dc_http_auth_type_t;

/**
 * @brief Supported Content-Type values
 */
typedef enum {
    DC_HTTP_CONTENT_TYPE_JSON,
    DC_HTTP_CONTENT_TYPE_FORM_URLENCODED,
    DC_HTTP_CONTENT_TYPE_MULTIPART
} dc_http_content_type_t;

/**
 * @brief Boolean query formatting styles
 */
typedef enum {
    DC_HTTP_BOOL_TRUE_FALSE,
    DC_HTTP_BOOL_ONE_ZERO
} dc_http_bool_format_t;

/**
 * @brief Parsed error structure from Discord HTTP responses
 */
typedef struct {
    int code;              /**< Discord error code (0 if absent) */
    dc_string_t message;   /**< Error message */
    dc_string_t errors;    /**< Raw JSON for "errors" object (may be empty) */
} dc_http_error_t;

/**
 * @brief Check if URL uses the Discord API base with explicit versioning
 * @param url URL to validate
 * @return 1 if valid, 0 otherwise
 */
int dc_http_is_discord_api_url(const char* url);

/**
 * @brief Build a full Discord API URL from a path or validate a full URL
 * @param path Path (e.g., "/users/@me") or full URL
 * @param out Output string
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_build_discord_api_url(const char* path, dc_string_t* out);

/**
 * @brief Format User-Agent string (DiscordBot (url, version) + optional suffix)
 * @param ua User-Agent descriptor
 * @param out Output string
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_format_user_agent(const dc_user_agent_t* ua, dc_string_t* out);

/**
 * @brief Format default User-Agent using library macros
 * @param out Output string
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_format_default_user_agent(dc_string_t* out);

/**
 * @brief Get the Content-Type string for a supported type
 * @param type Content-Type enum
 * @return String literal for the Content-Type
 */
const char* dc_http_content_type_string(dc_http_content_type_t type);

/**
 * @brief Validate Content-Type against allowed Discord values
 * @param content_type Content-Type header value
 * @return 1 if allowed, 0 otherwise
 */
int dc_http_content_type_is_allowed(const char* content_type);

/**
 * @brief Validate User-Agent string format
 * @param value User-Agent header value
 * @return 1 if valid, 0 otherwise
 */
int dc_http_user_agent_is_valid(const char* value);

/**
 * @brief Format Authorization header value
 * @param type Authorization type
 * @param token Token string
 * @param out Output string (e.g., "Bot <token>")
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_format_auth_header(dc_http_auth_type_t type, const char* token, dc_string_t* out);

/**
 * @brief Append a boolean query parameter to a query string
 * @param query Query string (will be modified)
 * @param key Parameter name
 * @param value Boolean value (0 or 1)
 * @param format Formatting style
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_append_query_bool(dc_string_t* query, const char* key, int value,
                                      dc_http_bool_format_t format);

/**
 * @brief Initialize an error struct
 * @param err Error struct
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_error_init(dc_http_error_t* err);

/**
 * @brief Free an error struct
 * @param err Error struct
 */
void dc_http_error_free(dc_http_error_t* err);

/**
 * @brief Parse Discord error response JSON
 * @param body Response body (JSON)
 * @param body_len Length of body (0 to auto-calc)
 * @param err Error struct to populate
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_error_parse(const char* body, size_t body_len, dc_http_error_t* err);

/**
 * @brief Validate that a buffer is valid JSON
 * @param body JSON text
 * @param body_len Length of JSON text (0 to auto-calc)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_validate_json_body(const char* body, size_t body_len);

/**
 * @brief Rate limit information from response headers
 */
typedef enum {
    DC_HTTP_RATE_LIMIT_SCOPE_UNKNOWN = 0,
    DC_HTTP_RATE_LIMIT_SCOPE_USER,
    DC_HTTP_RATE_LIMIT_SCOPE_GLOBAL,
    DC_HTTP_RATE_LIMIT_SCOPE_SHARED
} dc_http_rate_limit_scope_t;

typedef struct dc_http_rate_limit {
    int limit;              /**< Max requests per bucket (0 if absent) */
    int remaining;          /**< Remaining requests (0 if absent) */
    double reset;           /**< Unix timestamp when limit resets (0.0 if absent) */
    double reset_after;     /**< Seconds until reset (0.0 if absent) */
    double retry_after;     /**< Retry-After seconds (0.0 if absent) */
    dc_string_t bucket;     /**< Rate limit bucket ID (empty if absent) */
    int global;             /**< 1 if global rate limit, 0 otherwise */
    dc_http_rate_limit_scope_t scope; /**< Scope if provided */
} dc_http_rate_limit_t;

/**
 * @brief Initialize rate limit struct
 * @param rl Rate limit struct
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_rate_limit_init(dc_http_rate_limit_t* rl);

/**
 * @brief Free rate limit struct
 * @param rl Rate limit struct
 */
void dc_http_rate_limit_free(dc_http_rate_limit_t* rl);

/**
 * @brief Parse rate limit headers from response
 * @param get_header Callback to get header value by name
 * @param userdata User data for callback
 * @param rl Rate limit struct to populate
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_rate_limit_parse(
    dc_status_t (*get_header)(void* userdata, const char* name, const char** value),
    void* userdata,
    dc_http_rate_limit_t* rl);

/**
 * @brief Rate limit 429 response body
 */
typedef struct {
    dc_string_t message; /**< Message text */
    double retry_after;  /**< Seconds to wait before retry */
    int global;          /**< 1 if global rate limit, 0 otherwise */
    int code;            /**< Optional error code (0 if absent) */
} dc_http_rate_limit_response_t;

/**
 * @brief Initialize rate limit response struct
 * @param rl Rate limit response struct
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_rate_limit_response_init(dc_http_rate_limit_response_t* rl);

/**
 * @brief Free rate limit response struct
 * @param rl Rate limit response struct
 */
void dc_http_rate_limit_response_free(dc_http_rate_limit_response_t* rl);

/**
 * @brief Parse 429 response JSON body
 * @param body JSON body
 * @param body_len Length of body (0 to auto-calc)
 * @param rl Rate limit response struct to populate
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_http_rate_limit_response_parse(const char* body, size_t body_len,
                                              dc_http_rate_limit_response_t* rl);

#ifdef __cplusplus
}
#endif

#endif /* DC_HTTP_COMPLIANCE_H */
