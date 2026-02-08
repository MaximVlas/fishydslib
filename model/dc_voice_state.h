#ifndef DC_VOICE_STATE_H
#define DC_VOICE_STATE_H

/**
 * @file dc_voice_state.h
 * @brief Discord Voice State model (partial, as used in GUILD_CREATE)
 */

#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_snowflake.h"
#include "model/dc_model_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Voice State structure
 *
 * Represents a user's voice connection status.
 * In GUILD_CREATE events, this is a partial object lacking guild_id.
 */
typedef struct {
    dc_optional_snowflake_t guild_id;       /**< Guild ID (absent in GUILD_CREATE) */
    dc_snowflake_t channel_id;              /**< Channel ID user is connected to */
    dc_snowflake_t user_id;                 /**< User ID */
    dc_string_t session_id;                 /**< Voice session ID */
    int deaf;                               /**< Guild deafened */
    int mute;                               /**< Guild muted */
    int self_deaf;                          /**< Self deafened */
    int self_mute;                          /**< Self muted */
    int self_stream;                        /**< Streaming using Go Live */
    int self_video;                         /**< Camera enabled */
    int suppress;                           /**< Suppressed (stage channel) */
    dc_nullable_string_t request_to_speak_timestamp; /**< Request to speak timestamp */
} dc_voice_state_t;

/**
 * @brief Initialize voice state structure
 * @param vs Voice state to initialize
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_voice_state_init(dc_voice_state_t* vs);

/**
 * @brief Free voice state structure
 * @param vs Voice state to free
 */
void dc_voice_state_free(dc_voice_state_t* vs);

#ifdef __cplusplus
}
#endif

#endif /* DC_VOICE_STATE_H */
