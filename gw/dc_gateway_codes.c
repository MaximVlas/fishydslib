/**
 * @file dc_gateway_codes.c
 * @brief Gateway close code helpers
 */

#include "dc_gateway.h"

const char* dc_gateway_close_code_string(int code) {
    switch (code) {
        case DC_GATEWAY_CLOSE_UNKNOWN_ERROR:         return "Unknown error";
        case DC_GATEWAY_CLOSE_UNKNOWN_OPCODE:        return "Unknown opcode";
        case DC_GATEWAY_CLOSE_DECODE_ERROR:          return "Decode error";
        case DC_GATEWAY_CLOSE_NOT_AUTHENTICATED:     return "Not authenticated";
        case DC_GATEWAY_CLOSE_AUTHENTICATION_FAILED: return "Authentication failed";
        case DC_GATEWAY_CLOSE_ALREADY_AUTHENTICATED: return "Already authenticated";
        case DC_GATEWAY_CLOSE_INVALID_SEQ:           return "Invalid seq";
        case DC_GATEWAY_CLOSE_RATE_LIMITED:          return "Rate limited";
        case DC_GATEWAY_CLOSE_SESSION_TIMED_OUT:     return "Session timed out";
        case DC_GATEWAY_CLOSE_INVALID_SHARD:         return "Invalid shard";
        case DC_GATEWAY_CLOSE_SHARDING_REQUIRED:     return "Sharding required";
        case DC_GATEWAY_CLOSE_INVALID_API_VERSION:   return "Invalid API version";
        case DC_GATEWAY_CLOSE_INVALID_INTENTS:       return "Invalid intent(s)";
        case DC_GATEWAY_CLOSE_DISALLOWED_INTENTS:    return "Disallowed intent(s)";
        default:                                     return "Unknown close code";
    }
}

int dc_gateway_close_code_should_reconnect(int code) {
    switch (code) {
        case DC_GATEWAY_CLOSE_AUTHENTICATION_FAILED:
        case DC_GATEWAY_CLOSE_INVALID_SHARD:
        case DC_GATEWAY_CLOSE_SHARDING_REQUIRED:
        case DC_GATEWAY_CLOSE_INVALID_API_VERSION:
        case DC_GATEWAY_CLOSE_INVALID_INTENTS:
        case DC_GATEWAY_CLOSE_DISALLOWED_INTENTS:
            return 0;
        default:
            return 1;
    }
}
