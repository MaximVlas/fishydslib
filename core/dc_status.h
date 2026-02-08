#ifndef DC_STATUS_H
#define DC_STATUS_H

/**
 * @file dc_status.h
 * @brief Status codes and error handling for fishydslib
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status codes for all fishydslib operations
 */
typedef enum {
    DC_OK = 0,                    /**< Success */
    DC_ERROR_INVALID_PARAM,       /**< Invalid parameter */
    DC_ERROR_NULL_POINTER,        /**< Null pointer passed */
    DC_ERROR_OUT_OF_MEMORY,       /**< Memory allocation failed */
    DC_ERROR_BUFFER_TOO_SMALL,    /**< Buffer too small */
    DC_ERROR_INVALID_FORMAT,      /**< Invalid format */
    DC_ERROR_PARSE_ERROR,         /**< Parse error */
    DC_ERROR_NETWORK,             /**< Network error */
    DC_ERROR_HTTP,                /**< HTTP error */
    DC_ERROR_WEBSOCKET,           /**< WebSocket error */
    DC_ERROR_JSON,                /**< JSON error */
    DC_ERROR_RATE_LIMITED,        /**< Rate limited */
    DC_ERROR_UNAUTHORIZED,        /**< Unauthorized */
    DC_ERROR_FORBIDDEN,           /**< Forbidden */
    DC_ERROR_NOT_FOUND,           /**< Not found */
    DC_ERROR_TIMEOUT,             /**< Timeout */
    DC_ERROR_NOT_IMPLEMENTED,     /**< Not implemented */
    DC_ERROR_UNKNOWN,             /**< Unknown error */
    DC_ERROR_BAD_REQUEST,         /**< HTTP 400 */
    DC_ERROR_NOT_MODIFIED,         /**< HTTP 304 */
    DC_ERROR_METHOD_NOT_ALLOWED,   /**< HTTP 405 */
    DC_ERROR_CONFLICT,             /**< Conflict */
    DC_ERROR_UNAVAILABLE,          /**< HTTP 502/503 */
    DC_ERROR_SERVER,               /**< HTTP 5xx */
    DC_ERROR_INVALID_STATE,        /**< Invalid state */
    DC_ERROR_TRY_AGAIN             /**< Temporary failure */
} dc_status_t;

/**
 * @brief Result type that combines status and optional value
 */
#define DC_RESULT(T) struct { dc_status_t status; T value; }

/**
 * @brief Create a successful result
 */
#define DC_OK_RESULT(val) { .status = DC_OK, .value = (val) }

/**
 * @brief Create an error result
 */
#define DC_ERROR_RESULT(err) { .status = (err), .value = {0} }

/**
 * @brief Check if result is successful
 */
#define DC_IS_OK(result) ((result).status == DC_OK)

/**
 * @brief Check if result is an error
 */
#define DC_IS_ERROR(result) ((result).status != DC_OK)

/**
 * @brief Get the value from a result (only use if DC_IS_OK)
 */
#define DC_VALUE(result) ((result).value)

/**
 * @brief Get the status from a result
 */
#define DC_STATUS(result) ((result).status)

/**
 * @brief Convert status code to human-readable string
 * @param status The status code
 * @return String representation of the status
 */
const char* dc_status_string(dc_status_t status);

/**
 * @brief Check if a status represents a recoverable error
 * @param status The status code
 * @return 1 if recoverable, 0 otherwise
 */
int dc_status_is_recoverable(dc_status_t status);

/**
 * @brief Map HTTP status code to dc_status_t
 * @param http_status HTTP status code
 * @return Mapped dc_status_t
 */
dc_status_t dc_status_from_http(long http_status);

#ifdef __cplusplus
}
#endif

#endif /* DC_STATUS_H */
