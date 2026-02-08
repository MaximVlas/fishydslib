/**
 * @file dc_guild_member.c
 * @brief Guild member model implementation
 */

#include "dc_guild_member.h"
#include "json/dc_json.h"
#include "json/dc_json_model.h"
#include <string.h>

dc_status_t dc_guild_member_init(dc_guild_member_t* member) {
    if (!member) return DC_ERROR_NULL_POINTER;
    memset(member, 0, sizeof(*member));

    dc_status_t st = dc_user_init(&member->user);
    if (st != DC_OK) return st;
    st = dc_nullable_string_init(&member->nick);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&member->avatar);
    if (st != DC_OK) goto fail;
    st = dc_vec_init(&member->roles, sizeof(dc_snowflake_t));
    if (st != DC_OK) goto fail;
    st = dc_string_init(&member->joined_at);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&member->premium_since);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&member->communication_disabled_until);
    if (st != DC_OK) goto fail;

    return DC_OK;

fail:
    dc_guild_member_free(member);
    return st;
}

void dc_guild_member_free(dc_guild_member_t* member) {
    if (!member) return;
    dc_user_free(&member->user);
    dc_nullable_string_free(&member->nick);
    dc_nullable_string_free(&member->avatar);
    dc_vec_free(&member->roles);
    dc_string_free(&member->joined_at);
    dc_nullable_string_free(&member->premium_since);
    dc_nullable_string_free(&member->communication_disabled_until);
    memset(member, 0, sizeof(*member));
}

dc_status_t dc_guild_member_from_json(const char* json_data, dc_guild_member_t* member) {
    if (!json_data || !member) return DC_ERROR_NULL_POINTER;

    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(json_data, &doc);
    if (st != DC_OK) return st;

    dc_guild_member_t tmp;
    st = dc_guild_member_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    st = dc_json_model_guild_member_from_val(doc.root, &tmp);
    dc_json_doc_free(&doc);
    if (st != DC_OK) {
        dc_guild_member_free(&tmp);
        return st;
    }

    *member = tmp;
    return DC_OK;
}

dc_status_t dc_guild_member_to_json(const dc_guild_member_t* member, dc_string_t* json_result) {
    if (!member || !json_result) return DC_ERROR_NULL_POINTER;

    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;

    st = dc_json_model_guild_member_to_mut(&doc, doc.root, member);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }

    st = dc_json_mut_doc_serialize(&doc, json_result);
    dc_json_mut_doc_free(&doc);
    return st;
}

dc_status_t dc_guild_member_list_init(dc_guild_member_list_t* list) {
    if (!list) return DC_ERROR_NULL_POINTER;
    memset(list, 0, sizeof(*list));
    return dc_vec_init(&list->items, sizeof(dc_guild_member_t));
}

void dc_guild_member_list_free(dc_guild_member_list_t* list) {
    if (!list) return;
    for (size_t i = 0; i < dc_vec_length(&list->items); i++) {
        dc_guild_member_t* member = (dc_guild_member_t*)dc_vec_at(&list->items, i);
        if (member) dc_guild_member_free(member);
    }
    dc_vec_free(&list->items);
    memset(list, 0, sizeof(*list));
}
