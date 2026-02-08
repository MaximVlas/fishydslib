#ifndef DC_EVENTS_H
#define DC_EVENTS_H

/**
 * @file dc_events.h
 * @brief Gateway event parsing helpers
 */

/**
 * @note Use these helpers inside dc_gateway_event_callback_t; they expect the event "d" payload.
 */

#include "core/dc_status.h"
#include "core/dc_vec.h"
#include "model/dc_channel.h"
#include "model/dc_user.h"
#include "model/dc_guild.h"
#include "model/dc_guild_member.h"
#include "model/dc_message.h"
#include "model/dc_voice_state.h"
#include "model/dc_presence.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DC_GATEWAY_EVENT_UNKNOWN = 0,
    DC_GATEWAY_EVENT_THREAD_CREATE,
    DC_GATEWAY_EVENT_THREAD_UPDATE,
    DC_GATEWAY_EVENT_THREAD_DELETE,
    DC_GATEWAY_EVENT_THREAD_LIST_SYNC,
    DC_GATEWAY_EVENT_THREAD_MEMBER_UPDATE,
    DC_GATEWAY_EVENT_THREAD_MEMBERS_UPDATE,
    DC_GATEWAY_EVENT_READY,
    DC_GATEWAY_EVENT_GUILD_CREATE,
    DC_GATEWAY_EVENT_MESSAGE_CREATE
} dc_gateway_event_kind_t;

dc_gateway_event_kind_t dc_gateway_event_kind_from_name(const char* name);
int dc_gateway_event_is_thread_event(const char* name);

/**
 * @brief Gateway READY event data
 */
typedef struct {
    dc_snowflake_t id; /**< Guild ID */
    int unavailable;   /**< Unavailable flag (usually true in READY.guilds[]) */
} dc_gateway_unavailable_guild_t;

typedef struct {
    int v;                             /**< Gateway version */
    dc_user_t user;                    /**< Current user */
    dc_vec_t guilds;                   /**< Unavailable guilds (dc_gateway_unavailable_guild_t) */
    dc_string_t session_id;            /**< Session ID */
    dc_string_t resume_gateway_url;    /**< Resume gateway URL */
    dc_vec_t shard;                    /**< Shard info [shard_id, num_shards] */
    dc_optional_snowflake_t application_id; /**< Application ID */
} dc_gateway_ready_t;

dc_status_t dc_gateway_ready_init(dc_gateway_ready_t* ready);
void dc_gateway_ready_free(dc_gateway_ready_t* ready);
dc_status_t dc_gateway_event_parse_ready(const char* event_data, dc_gateway_ready_t* ready);

/**
 * @brief Gateway GUILD_CREATE event data
 *
 * Distinct from REST Guild object: contains members, channels, threads for initial cache population.
 */
typedef struct {
    dc_guild_t guild;           /**< Core guild object */
    dc_string_t joined_at;      /**< When user joined */
    int large;                  /**< Large guild flag */
    int unavailable;            /**< Unavailable flag */
    int member_count;           /**< Total member count */
    dc_vec_t voice_states;      /**< Voice states (dc_voice_state_t) */
    dc_vec_t members;           /**< Guild members (dc_guild_member_t) */
    dc_vec_t channels;          /**< Channels (dc_channel_t) */
    dc_vec_t threads;           /**< Active threads (dc_channel_t) */
    dc_vec_t presences;         /**< Presences (dc_presence_t) */
    dc_vec_t stage_instances;   /**< Stage instances (TODO: not parsed) */
    dc_vec_t guild_scheduled_events; /**< Scheduled events (TODO: not parsed) */
} dc_gateway_guild_create_t;

dc_status_t dc_gateway_guild_create_init(dc_gateway_guild_create_t* guild);
void dc_gateway_guild_create_free(dc_gateway_guild_create_t* guild);
dc_status_t dc_gateway_event_parse_guild_create(const char* event_data, dc_gateway_guild_create_t* guild);

/**
 * @brief Gateway MESSAGE_CREATE event data
 *
 * Wraps dc_message_t with gateway-specific extra fields (guild_id, member).
 */
typedef struct {
    dc_message_t message;           /**< Core message object */
    dc_optional_snowflake_t guild_id; /**< Guild ID (absent for DMs/ephemeral) */
    int has_member;                 /**< Whether member is present */
    dc_guild_member_t member;       /**< Partial guild member (author) */
} dc_gateway_message_create_t;

dc_status_t dc_gateway_message_create_init(dc_gateway_message_create_t* msg);
void dc_gateway_message_create_free(dc_gateway_message_create_t* msg);

/**
 * @brief Parse MESSAGE_CREATE event with full gateway fields
 * @param event_data JSON payload (event "d" object)
 * @param msg Output message with gateway-specific fields
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_gateway_event_parse_message_create_full(const char* event_data,
                                                        dc_gateway_message_create_t* msg);

/**
 * @brief Parse MESSAGE_CREATE event (legacy, message-only)
 * @deprecated Use dc_gateway_event_parse_message_create_full for guild_id and member
 */
dc_status_t dc_gateway_event_parse_message_create(const char* event_data, dc_message_t* message);

/**
 * @brief Parse THREAD_CREATE/THREAD_UPDATE/THREAD_DELETE payload into a channel model.
 * @param event_data JSON payload (event "d" object)
 * @param channel Output channel model (replaces contents on success)
 * @return DC_OK on success, error code on failure
 *
 * @note Caller owns the returned channel on success and must free it.
 */
dc_status_t dc_gateway_event_parse_thread_channel(const char* event_data, dc_channel_t* channel);

/**
 * @brief Parse THREAD_MEMBER_UPDATE payload into a thread member model.
 * @param event_data JSON payload (event "d" object)
 * @param member Output thread member model (replaces contents on success)
 * @return DC_OK on success, error code on failure
 *
 * @note Caller owns the returned member on success and must free it.
 */
dc_status_t dc_gateway_event_parse_thread_member(const char* event_data, dc_channel_thread_member_t* member);

typedef struct {
    dc_optional_snowflake_t guild_id;
    dc_optional_snowflake_t thread_id;
    dc_vec_t members;             /**< dc_channel_thread_member_t */
    dc_vec_t removed_member_ids;  /**< dc_snowflake_t */
} dc_gateway_thread_members_update_t;

dc_status_t dc_gateway_thread_members_update_init(dc_gateway_thread_members_update_t* update);
void dc_gateway_thread_members_update_free(dc_gateway_thread_members_update_t* update);

/**
 * @brief Parse THREAD_MEMBERS_UPDATE payload.
 * @param event_data JSON payload (event "d" object)
 * @param update Output update (replaces contents on success)
 * @return DC_OK on success, error code on failure
 *
 * @note Caller owns the returned update on success and must free it.
 */
dc_status_t dc_gateway_event_parse_thread_members_update(const char* event_data,
                                                         dc_gateway_thread_members_update_t* update);

typedef struct {
    dc_optional_snowflake_t guild_id;
    dc_vec_t channel_ids; /**< dc_snowflake_t */
    dc_vec_t threads;     /**< dc_channel_t */
    dc_vec_t members;     /**< dc_channel_thread_member_t */
} dc_gateway_thread_list_sync_t;

dc_status_t dc_gateway_thread_list_sync_init(dc_gateway_thread_list_sync_t* sync);
void dc_gateway_thread_list_sync_free(dc_gateway_thread_list_sync_t* sync);

/**
 * @brief Parse THREAD_LIST_SYNC payload.
 * @param event_data JSON payload (event "d" object)
 * @param sync Output sync data (replaces contents on success)
 * @return DC_OK on success, error code on failure
 *
 * @note Caller owns the returned sync data on success and must free it.
 */
dc_status_t dc_gateway_event_parse_thread_list_sync(const char* event_data,
                                                    dc_gateway_thread_list_sync_t* sync);

#ifdef __cplusplus
}
#endif

#endif /* DC_EVENTS_H */
