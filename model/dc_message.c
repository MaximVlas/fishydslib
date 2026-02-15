/**
 * @file dc_message.c
 * @brief Message model implementation
 */

#include "dc_message.h"
#include "json/dc_json.h"
#include "json/dc_json_model.h"
#include "core/dc_alloc.h"
#include <string.h>

dc_status_t dc_message_reference_init(dc_message_reference_t* ref) {
    if (!ref) return DC_ERROR_NULL_POINTER;
    memset(ref, 0, sizeof(*ref));
    ref->fail_if_not_exists = 1;
    return DC_OK;
}

void dc_message_reference_free(dc_message_reference_t* ref) {
    if (!ref) return;
    memset(ref, 0, sizeof(*ref));
}

dc_status_t dc_reaction_init(dc_reaction_t* reaction) {
    if (!reaction) return DC_ERROR_NULL_POINTER;
    memset(reaction, 0, sizeof(*reaction));
    dc_status_t st = dc_string_init(&reaction->emoji_name);
    if (st != DC_OK) return st;
    st = dc_vec_init(&reaction->burst_colors, sizeof(dc_string_t));
    if (st != DC_OK) return st;
    return DC_OK;
}

void dc_reaction_free(dc_reaction_t* reaction) {
    if (!reaction) return;
    dc_string_free(&reaction->emoji_name);
    for (size_t i = 0; i < dc_vec_length(&reaction->burst_colors); i++) {
        dc_string_t* s = (dc_string_t*)dc_vec_at(&reaction->burst_colors, i);
        if (s) dc_string_free(s);
    }
    dc_vec_free(&reaction->burst_colors);
    memset(reaction, 0, sizeof(*reaction));
}

dc_status_t dc_sticker_item_init(dc_sticker_item_t* item) {
    if (!item) return DC_ERROR_NULL_POINTER;
    memset(item, 0, sizeof(*item));
    return dc_string_init(&item->name);
}

void dc_sticker_item_free(dc_sticker_item_t* item) {
    if (!item) return;
    dc_string_free(&item->name);
    memset(item, 0, sizeof(*item));
}

dc_status_t dc_channel_mention_init(dc_channel_mention_t* mention) {
    if (!mention) return DC_ERROR_NULL_POINTER;
    memset(mention, 0, sizeof(*mention));
    return dc_string_init(&mention->name);
}

void dc_channel_mention_free(dc_channel_mention_t* mention) {
    if (!mention) return;
    dc_string_free(&mention->name);
    memset(mention, 0, sizeof(*mention));
}

dc_status_t dc_role_subscription_data_init(dc_role_subscription_data_t* data) {
    if (!data) return DC_ERROR_NULL_POINTER;
    memset(data, 0, sizeof(*data));
    return dc_string_init(&data->tier_name);
}

void dc_role_subscription_data_free(dc_role_subscription_data_t* data) {
    if (!data) return;
    dc_string_free(&data->tier_name);
    memset(data, 0, sizeof(*data));
}

dc_status_t dc_message_call_init(dc_message_call_t* call) {
    if (!call) return DC_ERROR_NULL_POINTER;
    memset(call, 0, sizeof(*call));
    dc_status_t st = dc_vec_init(&call->participants, sizeof(dc_snowflake_t));
    if (st != DC_OK) return st;
    st = dc_nullable_string_init(&call->ended_timestamp);
    if (st != DC_OK) return st;
    return DC_OK;
}

void dc_message_call_free(dc_message_call_t* call) {
    if (!call) return;
    dc_vec_free(&call->participants);
    dc_nullable_string_free(&call->ended_timestamp);
    memset(call, 0, sizeof(*call));
}

dc_status_t dc_message_activity_init(dc_message_activity_t* activity) {
    if (!activity) return DC_ERROR_NULL_POINTER;
    memset(activity, 0, sizeof(*activity));
    return dc_optional_string_init(&activity->party_id);
}

void dc_message_activity_free(dc_message_activity_t* activity) {
    if (!activity) return;
    dc_optional_string_free(&activity->party_id);
    memset(activity, 0, sizeof(*activity));
}

dc_status_t dc_message_init(dc_message_t* message) {
    if (!message) return DC_ERROR_NULL_POINTER;
    memset(message, 0, sizeof(*message));
    dc_status_t st = dc_user_init(&message->author);
    if (st != DC_OK) return st;
    st = dc_channel_init(&message->thread);
    if (st != DC_OK) return st;
    st = dc_string_init(&message->content);
    if (st != DC_OK) return st;
    st = dc_string_init(&message->timestamp);
    if (st != DC_OK) return st;
    st = dc_nullable_string_init(&message->edited_timestamp);
    if (st != DC_OK) return st;
    st = dc_vec_init(&message->mention_roles, sizeof(dc_snowflake_t));
    if (st != DC_OK) return st;
    st = dc_vec_init(&message->components, sizeof(dc_component_t));
    if (st != DC_OK) return st;
    st = dc_vec_init(&message->attachments, sizeof(dc_attachment_t));
    if (st != DC_OK) return st;
    st = dc_vec_init(&message->embeds, sizeof(dc_embed_t));
    if (st != DC_OK) return st;
    st = dc_vec_init(&message->mentions, sizeof(dc_guild_member_t));
    if (st != DC_OK) return st;
    st = dc_message_reference_init(&message->message_reference);
    if (st != DC_OK) return st;
    st = dc_optional_string_init(&message->nonce);
    if (st != DC_OK) return st;
    st = dc_vec_init(&message->reactions, sizeof(dc_reaction_t));
    if (st != DC_OK) return st;
    st = dc_vec_init(&message->sticker_items, sizeof(dc_sticker_item_t));
    if (st != DC_OK) return st;
    st = dc_vec_init(&message->mention_channels, sizeof(dc_channel_mention_t));
    if (st != DC_OK) return st;
    st = dc_role_subscription_data_init(&message->role_subscription_data);
    if (st != DC_OK) return st;
    st = dc_message_call_init(&message->call);
    if (st != DC_OK) return st;
    st = dc_message_activity_init(&message->activity);
    if (st != DC_OK) return st;
    return DC_OK;
}

void dc_message_free(dc_message_t* message) {
    if (!message) return;
    dc_user_free(&message->author);
    dc_channel_free(&message->thread);
    dc_string_free(&message->content);
    dc_string_free(&message->timestamp);
    dc_nullable_string_free(&message->edited_timestamp);
    dc_vec_free(&message->mention_roles);
    
    // Free all components
    for (size_t i = 0; i < dc_vec_length(&message->components); i++) {
        dc_component_t* component = (dc_component_t*)dc_vec_at(&message->components, i);
        if (component) dc_component_free(component);
    }
    dc_vec_free(&message->components);

    for (size_t i = 0; i < dc_vec_length(&message->attachments); i++) {
        dc_attachment_t* attachment = (dc_attachment_t*)dc_vec_at(&message->attachments, i);
        if (attachment) dc_attachment_free(attachment);
    }
    dc_vec_free(&message->attachments);

    for (size_t i = 0; i < dc_vec_length(&message->embeds); i++) {
        dc_embed_t* embed = (dc_embed_t*)dc_vec_at(&message->embeds, i);
        if (embed) dc_embed_free(embed);
    }
    dc_vec_free(&message->embeds);

    for (size_t i = 0; i < dc_vec_length(&message->mentions); i++) {
        dc_guild_member_t* mention = (dc_guild_member_t*)dc_vec_at(&message->mentions, i);
        if (mention) dc_guild_member_free(mention);
    }
    dc_vec_free(&message->mentions);

    dc_message_reference_free(&message->message_reference);

    if (message->referenced_message) {
        dc_message_free(message->referenced_message);
        dc_free(message->referenced_message);
        message->referenced_message = NULL;
    }

    dc_optional_string_free(&message->nonce);

    for (size_t i = 0; i < dc_vec_length(&message->reactions); i++) {
        dc_reaction_t* reaction = (dc_reaction_t*)dc_vec_at(&message->reactions, i);
        if (reaction) dc_reaction_free(reaction);
    }
    dc_vec_free(&message->reactions);

    for (size_t i = 0; i < dc_vec_length(&message->sticker_items); i++) {
        dc_sticker_item_t* item = (dc_sticker_item_t*)dc_vec_at(&message->sticker_items, i);
        if (item) dc_sticker_item_free(item);
    }
    dc_vec_free(&message->sticker_items);

    for (size_t i = 0; i < dc_vec_length(&message->mention_channels); i++) {
        dc_channel_mention_t* cm = (dc_channel_mention_t*)dc_vec_at(&message->mention_channels, i);
        if (cm) dc_channel_mention_free(cm);
    }
    dc_vec_free(&message->mention_channels);

    dc_role_subscription_data_free(&message->role_subscription_data);
    dc_message_call_free(&message->call);
    dc_message_activity_free(&message->activity);
    
    memset(message, 0, sizeof(*message));
}

dc_status_t dc_message_from_json(const char* json_data, dc_message_t* message) {
    if (!json_data || !message) return DC_ERROR_NULL_POINTER;
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(json_data, &doc);
    if (st != DC_OK) return st;

    dc_message_t tmp;
    st = dc_message_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    st = dc_json_model_message_from_val(doc.root, &tmp);
    dc_json_doc_free(&doc);
    if (st != DC_OK) {
        dc_message_free(&tmp);
        return st;
    }
    *message = tmp;
    return DC_OK;
}

dc_status_t dc_message_to_json(const dc_message_t* message, dc_string_t* json_result) {
    if (!message || !json_result) return DC_ERROR_NULL_POINTER;
    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;

    st = dc_json_model_message_to_mut(&doc, doc.root, message);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }

    st = dc_json_mut_doc_serialize(&doc, json_result);
    dc_json_mut_doc_free(&doc);
    return st;
}
