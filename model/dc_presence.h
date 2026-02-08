#ifndef DC_PRESENCE_H
#define DC_PRESENCE_H

/**
 * @file dc_presence.h
 * @brief Discord Presence model (partial, as used in GUILD_CREATE)
 */

#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_snowflake.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Presence status values
 */
typedef enum {
    DC_PRESENCE_STATUS_ONLINE = 0,
    DC_PRESENCE_STATUS_IDLE,
    DC_PRESENCE_STATUS_DND,
    DC_PRESENCE_STATUS_OFFLINE
} dc_presence_status_t;

/**
 * @brief Presence structure (partial)
 *
 * Represents a user's presence/status in a guild.
 * This is a simplified version; full presence includes activities and client_status.
 */
typedef struct {
    dc_snowflake_t user_id;             /**< User ID (from user.id) */
    dc_presence_status_t status;         /**< Status enum */
    dc_string_t status_str;              /**< Raw status string */
} dc_presence_t;

/**
 * @brief Initialize presence structure
 * @param presence Presence to initialize
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_presence_init(dc_presence_t* presence);

/**
 * @brief Free presence structure
 * @param presence Presence to free
 */
void dc_presence_free(dc_presence_t* presence);

/**
 * @brief Parse status string to enum
 * @param status_str Status string ("online", "idle", "dnd", "offline")
 * @return Corresponding enum value
 */
dc_presence_status_t dc_presence_status_from_string(const char* status_str);

#ifdef __cplusplus
}
#endif

#endif /* DC_PRESENCE_H */
