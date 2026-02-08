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
    DC_GATEWAY_EVENT_THREAD_MEMBERS_UPDATE
} dc_gateway_event_kind_t;

dc_gateway_event_kind_t dc_gateway_event_kind_from_name(const char* name);
int dc_gateway_event_is_thread_event(const char* name);

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
