#ifndef DC_PERMISSIONS_H
#define DC_PERMISSIONS_H

/**
 * @file dc_permissions.h
 * @brief Discord permission bit flags and permission computation helpers
 *
 * This follows the Discord API v10 permissions topic documentation.
 * Permissions are represented as a bitfield serialized as a decimal string in JSON.
 */

#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_snowflake.h"
#include "model/dc_channel.h"
#include "model/dc_guild_member.h"
#include "model/dc_role.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t dc_permissions_t;

#define DC_PERMISSIONS_NONE ((dc_permissions_t)0)
#define DC_PERMISSIONS_ALL  ((dc_permissions_t)UINT64_MAX)

/* Bitwise permission flags (Discord API v10) */
#define DC_PERMISSION_CREATE_INSTANT_INVITE                  ((dc_permissions_t)1ULL << 0)
#define DC_PERMISSION_KICK_MEMBERS                           ((dc_permissions_t)1ULL << 1)
#define DC_PERMISSION_BAN_MEMBERS                            ((dc_permissions_t)1ULL << 2)
#define DC_PERMISSION_ADMINISTRATOR                          ((dc_permissions_t)1ULL << 3)
#define DC_PERMISSION_MANAGE_CHANNELS                        ((dc_permissions_t)1ULL << 4)
#define DC_PERMISSION_MANAGE_GUILD                           ((dc_permissions_t)1ULL << 5)
#define DC_PERMISSION_ADD_REACTIONS                          ((dc_permissions_t)1ULL << 6)
#define DC_PERMISSION_VIEW_AUDIT_LOG                         ((dc_permissions_t)1ULL << 7)
#define DC_PERMISSION_PRIORITY_SPEAKER                       ((dc_permissions_t)1ULL << 8)
#define DC_PERMISSION_STREAM                                 ((dc_permissions_t)1ULL << 9)
#define DC_PERMISSION_VIEW_CHANNEL                           ((dc_permissions_t)1ULL << 10)
#define DC_PERMISSION_SEND_MESSAGES                          ((dc_permissions_t)1ULL << 11)
#define DC_PERMISSION_SEND_TTS_MESSAGES                      ((dc_permissions_t)1ULL << 12)
#define DC_PERMISSION_MANAGE_MESSAGES                        ((dc_permissions_t)1ULL << 13)
#define DC_PERMISSION_EMBED_LINKS                            ((dc_permissions_t)1ULL << 14)
#define DC_PERMISSION_ATTACH_FILES                           ((dc_permissions_t)1ULL << 15)
#define DC_PERMISSION_READ_MESSAGE_HISTORY                   ((dc_permissions_t)1ULL << 16)
#define DC_PERMISSION_MENTION_EVERYONE                       ((dc_permissions_t)1ULL << 17)
#define DC_PERMISSION_USE_EXTERNAL_EMOJIS                    ((dc_permissions_t)1ULL << 18)
#define DC_PERMISSION_VIEW_GUILD_INSIGHTS                    ((dc_permissions_t)1ULL << 19)
#define DC_PERMISSION_CONNECT                                ((dc_permissions_t)1ULL << 20)
#define DC_PERMISSION_SPEAK                                  ((dc_permissions_t)1ULL << 21)
#define DC_PERMISSION_MUTE_MEMBERS                           ((dc_permissions_t)1ULL << 22)
#define DC_PERMISSION_DEAFEN_MEMBERS                         ((dc_permissions_t)1ULL << 23)
#define DC_PERMISSION_MOVE_MEMBERS                           ((dc_permissions_t)1ULL << 24)
#define DC_PERMISSION_USE_VAD                                ((dc_permissions_t)1ULL << 25)
#define DC_PERMISSION_CHANGE_NICKNAME                        ((dc_permissions_t)1ULL << 26)
#define DC_PERMISSION_MANAGE_NICKNAMES                       ((dc_permissions_t)1ULL << 27)
#define DC_PERMISSION_MANAGE_ROLES                           ((dc_permissions_t)1ULL << 28)
#define DC_PERMISSION_MANAGE_WEBHOOKS                        ((dc_permissions_t)1ULL << 29)
#define DC_PERMISSION_MANAGE_GUILD_EXPRESSIONS               ((dc_permissions_t)1ULL << 30)
#define DC_PERMISSION_USE_APPLICATION_COMMANDS               ((dc_permissions_t)1ULL << 31)
#define DC_PERMISSION_REQUEST_TO_SPEAK                       ((dc_permissions_t)1ULL << 32)
#define DC_PERMISSION_MANAGE_EVENTS                          ((dc_permissions_t)1ULL << 33)
#define DC_PERMISSION_MANAGE_THREADS                         ((dc_permissions_t)1ULL << 34)
#define DC_PERMISSION_CREATE_PUBLIC_THREADS                  ((dc_permissions_t)1ULL << 35)
#define DC_PERMISSION_CREATE_PRIVATE_THREADS                 ((dc_permissions_t)1ULL << 36)
#define DC_PERMISSION_USE_EXTERNAL_STICKERS                  ((dc_permissions_t)1ULL << 37)
#define DC_PERMISSION_SEND_MESSAGES_IN_THREADS               ((dc_permissions_t)1ULL << 38)
#define DC_PERMISSION_USE_EMBEDDED_ACTIVITIES                ((dc_permissions_t)1ULL << 39)
#define DC_PERMISSION_MODERATE_MEMBERS                       ((dc_permissions_t)1ULL << 40)
#define DC_PERMISSION_VIEW_CREATOR_MONETIZATION_ANALYTICS    ((dc_permissions_t)1ULL << 41)
#define DC_PERMISSION_USE_SOUNDBOARD                         ((dc_permissions_t)1ULL << 42)
#define DC_PERMISSION_CREATE_GUILD_EXPRESSIONS               ((dc_permissions_t)1ULL << 43)
#define DC_PERMISSION_CREATE_EVENTS                          ((dc_permissions_t)1ULL << 44)
#define DC_PERMISSION_USE_EXTERNAL_SOUNDS                    ((dc_permissions_t)1ULL << 45)
#define DC_PERMISSION_SEND_VOICE_MESSAGES                    ((dc_permissions_t)1ULL << 46)
#define DC_PERMISSION_SEND_POLLS                             ((dc_permissions_t)1ULL << 49)
#define DC_PERMISSION_USE_EXTERNAL_APPS                      ((dc_permissions_t)1ULL << 50)
#define DC_PERMISSION_PIN_MESSAGES                           ((dc_permissions_t)1ULL << 51)
#define DC_PERMISSION_BYPASS_SLOWMODE                        ((dc_permissions_t)1ULL << 52)

static inline int dc_permissions_has(dc_permissions_t perms, dc_permissions_t flag) {
    return (perms & flag) == flag;
}

/**
 * @brief Compute guild-level base permissions for a member (roles only).
 *
 * Follows the Discord algorithm:
 * - owner => ALL
 * - base = @everyone role permissions OR'ed with member role permissions
 * - ADMINISTRATOR => ALL
 */
dc_status_t dc_permissions_compute_base(dc_snowflake_t guild_id,
                                       dc_snowflake_t guild_owner_id,
                                       dc_snowflake_t member_user_id,
                                       const dc_role_list_t* roles,
                                       const dc_vec_t* member_role_ids, /* dc_snowflake_t */
                                       dc_permissions_t* out_permissions);

/**
 * @brief Apply channel permission overwrites to a base permission set.
 *
 * This does not apply implicit permissions or thread/timed-out special rules.
 */
dc_status_t dc_permissions_compute_overwrites(dc_permissions_t base_permissions,
                                             dc_snowflake_t guild_id,
                                             dc_snowflake_t member_user_id,
                                             const dc_vec_t* member_role_ids, /* dc_snowflake_t */
                                             const dc_vec_t* permission_overwrites, /* dc_permission_overwrite_t */
                                             dc_permissions_t* out_permissions);

/**
 * @brief Convenience wrapper: compute base + overwrites for a channel.
 */
dc_status_t dc_permissions_compute_channel(dc_snowflake_t guild_id,
                                          dc_snowflake_t guild_owner_id,
                                          const dc_role_list_t* roles,
                                          const dc_guild_member_t* member,
                                          const dc_channel_t* channel,
                                          dc_permissions_t* out_permissions);

/**
 * @brief Apply documented implicit permission rules for text-like channels.
 *
 * - Missing VIEW_CHANNEL => returns 0
 * - Missing SEND_MESSAGES => clears dependent send-related permissions
 */
dc_permissions_t dc_permissions_apply_implicit_text(dc_permissions_t perms);

/**
 * @brief Apply thread inheritance rule: SEND_MESSAGES is not inherited in threads.
 *
 * If @p channel_type is a thread type, SEND_MESSAGES is cleared.
 */
dc_permissions_t dc_permissions_apply_thread_rules(dc_permissions_t perms, dc_channel_type_t channel_type);

/**
 * @brief Apply timed-out member mask: only VIEW_CHANNEL and READ_MESSAGE_HISTORY remain.
 */
dc_permissions_t dc_permissions_apply_timed_out_mask(dc_permissions_t perms);

#ifdef __cplusplus
}
#endif

#endif /* DC_PERMISSIONS_H */

