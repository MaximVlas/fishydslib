#ifndef DC_GATEWAY_H
#define DC_GATEWAY_H

/**
 * @file dc_gateway.h
 * @brief Discord Gateway WebSocket client
 */

#include <stddef.h>
#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_snowflake.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Gateway connection state
 */
typedef enum {
    DC_GATEWAY_DISCONNECTED,
    DC_GATEWAY_CONNECTING,
    DC_GATEWAY_CONNECTED,
    DC_GATEWAY_IDENTIFYING,
    DC_GATEWAY_READY,
    DC_GATEWAY_RESUMING,
    DC_GATEWAY_RECONNECTING
} dc_gateway_state_t;

/**
 * @brief Gateway intents
 */
typedef enum {
    DC_INTENT_GUILDS                    = 1 << 0,
    DC_INTENT_GUILD_MEMBERS             = 1 << 1,
    DC_INTENT_GUILD_MODERATION          = 1 << 2,
    DC_INTENT_GUILD_EMOJIS_AND_STICKERS = 1 << 3,
    DC_INTENT_GUILD_INTEGRATIONS        = 1 << 4,
    DC_INTENT_GUILD_WEBHOOKS            = 1 << 5,
    DC_INTENT_GUILD_INVITES             = 1 << 6,
    DC_INTENT_GUILD_VOICE_STATES        = 1 << 7,
    DC_INTENT_GUILD_PRESENCES           = 1 << 8,
    DC_INTENT_GUILD_MESSAGES            = 1 << 9,
    DC_INTENT_GUILD_MESSAGE_REACTIONS   = 1 << 10,
    DC_INTENT_GUILD_MESSAGE_TYPING      = 1 << 11,
    DC_INTENT_DIRECT_MESSAGES           = 1 << 12,
    DC_INTENT_DIRECT_MESSAGE_REACTIONS  = 1 << 13,
    DC_INTENT_DIRECT_MESSAGE_TYPING     = 1 << 14,
    DC_INTENT_MESSAGE_CONTENT           = 1 << 15,
    DC_INTENT_GUILD_SCHEDULED_EVENTS    = 1 << 16,
    DC_INTENT_AUTO_MODERATION_CONFIG    = 1 << 20,
    DC_INTENT_AUTO_MODERATION_EXECUTION = 1 << 21
} dc_gateway_intent_t;

/**
 * @brief Gateway opcodes
 */
typedef enum {
    DC_GATEWAY_OP_DISPATCH              = 0,
    DC_GATEWAY_OP_HEARTBEAT             = 1,
    DC_GATEWAY_OP_IDENTIFY              = 2,
    DC_GATEWAY_OP_PRESENCE_UPDATE       = 3,
    DC_GATEWAY_OP_VOICE_STATE_UPDATE    = 4,
    DC_GATEWAY_OP_RESUME                = 6,
    DC_GATEWAY_OP_RECONNECT             = 7,
    DC_GATEWAY_OP_REQUEST_GUILD_MEMBERS = 8,
    DC_GATEWAY_OP_INVALID_SESSION       = 9,
    DC_GATEWAY_OP_HELLO                 = 10,
    DC_GATEWAY_OP_HEARTBEAT_ACK         = 11,
    DC_GATEWAY_OP_REQUEST_SOUNDBOARD_SOUNDS = 31
} dc_gateway_opcode_t;

/**
 * @brief Gateway close codes
 */
typedef enum {
    DC_GATEWAY_CLOSE_UNKNOWN_ERROR         = 4000,
    DC_GATEWAY_CLOSE_UNKNOWN_OPCODE        = 4001,
    DC_GATEWAY_CLOSE_DECODE_ERROR          = 4002,
    DC_GATEWAY_CLOSE_NOT_AUTHENTICATED     = 4003,
    DC_GATEWAY_CLOSE_AUTHENTICATION_FAILED = 4004,
    DC_GATEWAY_CLOSE_ALREADY_AUTHENTICATED = 4005,
    DC_GATEWAY_CLOSE_INVALID_SEQ           = 4007,
    DC_GATEWAY_CLOSE_RATE_LIMITED          = 4008,
    DC_GATEWAY_CLOSE_SESSION_TIMED_OUT     = 4009,
    DC_GATEWAY_CLOSE_INVALID_SHARD         = 4010,
    DC_GATEWAY_CLOSE_SHARDING_REQUIRED     = 4011,
    DC_GATEWAY_CLOSE_INVALID_API_VERSION   = 4012,
    DC_GATEWAY_CLOSE_INVALID_INTENTS       = 4013,
    DC_GATEWAY_CLOSE_DISALLOWED_INTENTS    = 4014
} dc_gateway_close_code_t;

/**
 * @brief Get human-readable text for a close code
 * @param code Close code
 * @return String description
 */
const char* dc_gateway_close_code_string(int code);

/**
 * @brief Determine if a close code is safe to reconnect
 * @param code Close code
 * @return 1 if reconnect is allowed, 0 otherwise
 */
int dc_gateway_close_code_should_reconnect(int code);

/**
 * @brief Gateway event callback function
 * @param event_name Event name (e.g., "MESSAGE_CREATE")
 * @param event_data Event data as JSON string
 * @param user_data User-provided data
 *
 * @note Callbacks run on the thread invoking dc_gateway_client_process and must not block.
 * @note Events can be duplicated or arrive out of order; handlers should be idempotent.
 * @note Dispatches with non-increasing sequence numbers are ignored to reduce duplicates.
 * @note Thread events (THREAD_*, THREAD_LIST_SYNC, THREAD_MEMBER_UPDATE, THREAD_MEMBERS_UPDATE)
 *       are delivered through this callback like any other dispatch.
 */
typedef void (*dc_gateway_event_callback_t)(const char* event_name, 
                                             const char* event_data, 
                                             void* user_data);

/**
 * @brief Gateway connection callback function
 * @param state New connection state
 * @param user_data User-provided data
 */
typedef void (*dc_gateway_state_callback_t)(dc_gateway_state_t state, void* user_data);

/**
 * @brief Gateway client structure
 */
typedef struct dc_gateway_client dc_gateway_client_t;

/**
 * @brief Gateway configuration
 */
typedef struct {
    const char* token;                          /**< Bot token */
    uint32_t intents;                           /**< Gateway intents */
    uint32_t shard_id;                          /**< Shard id (optional, requires shard_count) */
    uint32_t shard_count;                       /**< Total shards (optional, 0 to omit) */
    uint32_t large_threshold;                   /**< Identify large_threshold (50-250, 0 to omit) */
    const char* user_agent;                     /**< User agent string */
    dc_gateway_event_callback_t event_callback; /**< Event callback */
    dc_gateway_state_callback_t state_callback; /**< State callback */
    void* user_data;                            /**< User data for callbacks */
    uint32_t heartbeat_timeout_ms;              /**< Heartbeat timeout */
    uint32_t connect_timeout_ms;                /**< Connection timeout */
    int enable_compression;                     /**< Enable zlib-stream transport compression (JSON only) */
    int enable_payload_compression;             /**< Enable Identify payload compression (JSON only) */
} dc_gateway_config_t;

/**
 * @brief Create gateway client
 * @param config Gateway configuration
 * @param client Pointer to store created client
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_gateway_client_create(const dc_gateway_config_t* config, 
                                      dc_gateway_client_t** client);

/**
 * @brief Free gateway client
 * @param client Client to free
 */
void dc_gateway_client_free(dc_gateway_client_t* client);

/**
 * @brief Connect to gateway
 * @param client Gateway client
 * @param gateway_url Gateway URL (from /gateway/bot endpoint). If NULL, uses cached resume/base URL.
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_gateway_client_connect(dc_gateway_client_t* client, const char* gateway_url);

/**
 * @brief Disconnect from gateway
 * @param client Gateway client
 * @return DC_OK on success, error code on failure
 *
 * @note Stops automatic reconnect attempts; continue processing until disconnected.
 */
dc_status_t dc_gateway_client_disconnect(dc_gateway_client_t* client);

/**
 * @brief Process gateway events (call regularly in event loop)
 * @param client Gateway client
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking)
 * @return DC_OK on success, error code on failure
 *
 * @note This is the single-threaded event loop entry point; callbacks are invoked here.
 * @note Integrate by calling with a small timeout or 0 from your loop and sleeping externally.
 * @note Typical loop: while (running) { dc_gateway_client_process(client, 50); }
 */
dc_status_t dc_gateway_client_process(dc_gateway_client_t* client, uint32_t timeout_ms);

/**
 * @brief Get current gateway state
 * @param client Gateway client
 * @param state Pointer to store current state
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_gateway_client_get_state(const dc_gateway_client_t* client, 
                                         dc_gateway_state_t* state);

/**
 * @brief Send presence update
 * @param client Gateway client
 * @param status Status string ("online", "idle", "dnd", "invisible")
 * @param activity_name Activity name (optional)
 * @param activity_type Activity type (0=playing, 1=streaming, 2=listening, 3=watching, 5=competing)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_gateway_client_update_presence(dc_gateway_client_t* client,
                                               const char* status,
                                               const char* activity_name,
                                               int activity_type);

/**
 * @brief Request guild members (Gateway op 8)
 * @param client Gateway client
 * @param guild_id Guild ID
 * @param query Username prefix query, or "" for all members (mutually exclusive with user_ids)
 * @param limit Max members for query mode (required with query)
 * @param presences Non-zero to request presence objects
 * @param user_ids Optional list of user IDs to fetch (mutually exclusive with query)
 * @param user_id_count Number of entries in user_ids
 * @param nonce Optional nonce (max 32 bytes)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_gateway_client_request_guild_members(dc_gateway_client_t* client,
                                                    dc_snowflake_t guild_id,
                                                    const char* query,
                                                    uint32_t limit,
                                                    int presences,
                                                    const dc_snowflake_t* user_ids,
                                                    size_t user_id_count,
                                                    const char* nonce);

/**
 * @brief Request soundboard sounds for guilds (Gateway op 31)
 * @param client Gateway client
 * @param guild_ids Guild ID array
 * @param guild_id_count Number of guild IDs
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_gateway_client_request_soundboard_sounds(dc_gateway_client_t* client,
                                                        const dc_snowflake_t* guild_ids,
                                                        size_t guild_id_count);

/**
 * @brief Update voice state (Gateway op 4)
 * @param client Gateway client
 * @param guild_id Guild ID
 * @param channel_id Channel ID, or 0 to disconnect
 * @param self_mute Non-zero to self-mute
 * @param self_deaf Non-zero to self-deafen
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_gateway_client_update_voice_state(dc_gateway_client_t* client,
                                                 dc_snowflake_t guild_id,
                                                 dc_snowflake_t channel_id,
                                                 int self_mute,
                                                 int self_deaf);

#ifdef __cplusplus
}
#endif

#endif /* DC_GATEWAY_H */
