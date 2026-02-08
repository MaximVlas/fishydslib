/**
 * @file dc_permissions.c
 * @brief Permission bit helpers and effective-permissions computation
 */

#include "dc_permissions.h"
#include <string.h>

static const dc_role_t* dc_find_role(const dc_role_list_t* roles, dc_snowflake_t role_id) {
    if (!roles) return NULL;
    for (size_t i = 0; i < dc_vec_length(&roles->items); i++) {
        const dc_role_t* role = (const dc_role_t*)dc_vec_at(&roles->items, i);
        if (role && role->id == role_id) return role;
    }
    return NULL;
}

static const dc_permission_overwrite_t* dc_find_overwrite(const dc_vec_t* overwrites,
                                                          dc_snowflake_t id,
                                                          dc_permission_overwrite_type_t type) {
    if (!overwrites) return NULL;
    for (size_t i = 0; i < dc_vec_length(overwrites); i++) {
        const dc_permission_overwrite_t* ow = (const dc_permission_overwrite_t*)dc_vec_at(overwrites, i);
        if (ow && ow->id == id && ow->type == type) return ow;
    }
    return NULL;
}

dc_status_t dc_permissions_compute_base(dc_snowflake_t guild_id,
                                       dc_snowflake_t guild_owner_id,
                                       dc_snowflake_t member_user_id,
                                       const dc_role_list_t* roles,
                                       const dc_vec_t* member_role_ids,
                                       dc_permissions_t* out_permissions) {
    if (!roles || !out_permissions) return DC_ERROR_NULL_POINTER;

    if (guild_owner_id != 0 && member_user_id == guild_owner_id) {
        *out_permissions = DC_PERMISSIONS_ALL;
        return DC_OK;
    }

    const dc_role_t* everyone = dc_find_role(roles, guild_id);
    if (!everyone) return DC_ERROR_NOT_FOUND;

    dc_permissions_t perms = everyone->permissions;

    if (member_role_ids) {
        for (size_t i = 0; i < dc_vec_length(member_role_ids); i++) {
            const dc_snowflake_t* role_id = (const dc_snowflake_t*)dc_vec_at(member_role_ids, i);
            if (!role_id) return DC_ERROR_INVALID_STATE;
            const dc_role_t* role = dc_find_role(roles, *role_id);
            if (!role) return DC_ERROR_NOT_FOUND;
            perms |= role->permissions;
        }
    }

    if ((perms & DC_PERMISSION_ADMINISTRATOR) == DC_PERMISSION_ADMINISTRATOR) {
        *out_permissions = DC_PERMISSIONS_ALL;
        return DC_OK;
    }

    *out_permissions = perms;
    return DC_OK;
}

dc_status_t dc_permissions_compute_overwrites(dc_permissions_t base_permissions,
                                             dc_snowflake_t guild_id,
                                             dc_snowflake_t member_user_id,
                                             const dc_vec_t* member_role_ids,
                                             const dc_vec_t* permission_overwrites,
                                             dc_permissions_t* out_permissions) {
    if (!out_permissions) return DC_ERROR_NULL_POINTER;

    if ((base_permissions & DC_PERMISSION_ADMINISTRATOR) == DC_PERMISSION_ADMINISTRATOR) {
        *out_permissions = DC_PERMISSIONS_ALL;
        return DC_OK;
    }

    dc_permissions_t perms = base_permissions;

    const dc_permission_overwrite_t* everyone_ow =
        dc_find_overwrite(permission_overwrites, guild_id, DC_PERMISSION_OVERWRITE_TYPE_ROLE);
    if (everyone_ow) {
        perms &= ~everyone_ow->deny;
        perms |= everyone_ow->allow;
    }

    dc_permissions_t allow = DC_PERMISSIONS_NONE;
    dc_permissions_t deny = DC_PERMISSIONS_NONE;
    if (member_role_ids) {
        for (size_t i = 0; i < dc_vec_length(member_role_ids); i++) {
            const dc_snowflake_t* role_id = (const dc_snowflake_t*)dc_vec_at(member_role_ids, i);
            if (!role_id) return DC_ERROR_INVALID_STATE;
            const dc_permission_overwrite_t* role_ow =
                dc_find_overwrite(permission_overwrites, *role_id, DC_PERMISSION_OVERWRITE_TYPE_ROLE);
            if (role_ow) {
                allow |= role_ow->allow;
                deny |= role_ow->deny;
            }
        }
    }

    perms &= ~deny;
    perms |= allow;

    const dc_permission_overwrite_t* member_ow =
        dc_find_overwrite(permission_overwrites, member_user_id, DC_PERMISSION_OVERWRITE_TYPE_MEMBER);
    if (member_ow) {
        perms &= ~member_ow->deny;
        perms |= member_ow->allow;
    }

    *out_permissions = perms;
    return DC_OK;
}

dc_status_t dc_permissions_compute_channel(dc_snowflake_t guild_id,
                                          dc_snowflake_t guild_owner_id,
                                          const dc_role_list_t* roles,
                                          const dc_guild_member_t* member,
                                          const dc_channel_t* channel,
                                          dc_permissions_t* out_permissions) {
    if (!roles || !member || !channel || !out_permissions) return DC_ERROR_NULL_POINTER;
    if (!member->has_user) return DC_ERROR_INVALID_PARAM;

    dc_permissions_t base = DC_PERMISSIONS_NONE;
    dc_status_t st = dc_permissions_compute_base(guild_id, guild_owner_id, member->user.id, roles, &member->roles, &base);
    if (st != DC_OK) return st;

    return dc_permissions_compute_overwrites(base, guild_id, member->user.id, &member->roles,
                                             &channel->permission_overwrites, out_permissions);
}

dc_permissions_t dc_permissions_apply_implicit_text(dc_permissions_t perms) {
    if ((perms & DC_PERMISSION_VIEW_CHANNEL) != DC_PERMISSION_VIEW_CHANNEL) return DC_PERMISSIONS_NONE;

    if ((perms & DC_PERMISSION_SEND_MESSAGES) != DC_PERMISSION_SEND_MESSAGES) {
        perms &= ~DC_PERMISSION_MENTION_EVERYONE;
        perms &= ~DC_PERMISSION_SEND_TTS_MESSAGES;
        perms &= ~DC_PERMISSION_ATTACH_FILES;
        perms &= ~DC_PERMISSION_EMBED_LINKS;
    }

    return perms;
}

dc_permissions_t dc_permissions_apply_thread_rules(dc_permissions_t perms, dc_channel_type_t channel_type) {
    switch (channel_type) {
        case DC_CHANNEL_TYPE_ANNOUNCEMENT_THREAD:
        case DC_CHANNEL_TYPE_PUBLIC_THREAD:
        case DC_CHANNEL_TYPE_PRIVATE_THREAD:
            perms &= ~DC_PERMISSION_SEND_MESSAGES;
            break;
        default:
            break;
    }
    return perms;
}

dc_permissions_t dc_permissions_apply_timed_out_mask(dc_permissions_t perms) {
    return perms & (DC_PERMISSION_VIEW_CHANNEL | DC_PERMISSION_READ_MESSAGE_HISTORY);
}
