/**
 * @file dc_status.c
 * @brief Status codes and error handling implementation
 */

#include "dc_status.h"

const char* dc_status_string(dc_status_t status) {
    switch (status) {
        case DC_OK:                     return "Success";
        case DC_ERROR_INVALID_PARAM:    return "Invalid parameter";
        case DC_ERROR_NULL_POINTER:     return "Null pointer";
        case DC_ERROR_OUT_OF_MEMORY:    return "Out of memory";
        case DC_ERROR_BUFFER_TOO_SMALL: return "Buffer too small";
        case DC_ERROR_INVALID_FORMAT:   return "Invalid format";
        case DC_ERROR_PARSE_ERROR:      return "Parse error";
        case DC_ERROR_NETWORK:          return "Network error";
        case DC_ERROR_HTTP:             return "HTTP error";
        case DC_ERROR_WEBSOCKET:        return "WebSocket error";
        case DC_ERROR_JSON:             return "JSON error";
        case DC_ERROR_RATE_LIMITED:     return "Rate limited";
        case DC_ERROR_UNAUTHORIZED:     return "Unauthorized";
        case DC_ERROR_FORBIDDEN:        return "Forbidden";
        case DC_ERROR_NOT_FOUND:        return "Not found";
        case DC_ERROR_TIMEOUT:          return "Timeout";
        case DC_ERROR_NOT_IMPLEMENTED:  return "Not implemented";
        case DC_ERROR_UNKNOWN:          return "Unknown error";
        case DC_ERROR_BAD_REQUEST:      return "Bad request";
        case DC_ERROR_NOT_MODIFIED:     return "Not modified";
        case DC_ERROR_METHOD_NOT_ALLOWED:return "Method not allowed";
        case DC_ERROR_CONFLICT:         return "Conflict";
        case DC_ERROR_UNAVAILABLE:      return "Unavailable";
        case DC_ERROR_SERVER:           return "Server error";
        case DC_ERROR_INVALID_STATE:    return "Invalid state";
        case DC_ERROR_TRY_AGAIN:        return "Try again";
        default:                        return "Invalid status code";
    }
}

int dc_status_is_recoverable(dc_status_t status) {
    switch (status) {
        case DC_ERROR_NETWORK:
        case DC_ERROR_TIMEOUT:
        case DC_ERROR_RATE_LIMITED:
        case DC_ERROR_UNAVAILABLE:
        case DC_ERROR_SERVER:
        case DC_ERROR_TRY_AGAIN:
            return 1;
        default:
            return 0;
    }
}

dc_status_t dc_status_from_http(long http_status) {
    switch (http_status) {
        case 200:
        case 201:
        case 202:
        case 204:
            return DC_OK;
        case 304:
            return DC_ERROR_NOT_MODIFIED;
        case 400:
            return DC_ERROR_BAD_REQUEST;
        case 401:
            return DC_ERROR_UNAUTHORIZED;
        case 403:
            return DC_ERROR_FORBIDDEN;
        case 404:
            return DC_ERROR_NOT_FOUND;
        case 405:
            return DC_ERROR_METHOD_NOT_ALLOWED;
        case 409:
            return DC_ERROR_CONFLICT;
        case 429:
            return DC_ERROR_RATE_LIMITED;
        case 502:
        case 503:
            return DC_ERROR_UNAVAILABLE;
        default:
            if (http_status >= 500 && http_status <= 599) {
                return DC_ERROR_SERVER;
            }
            return DC_ERROR_HTTP;
    }
}
