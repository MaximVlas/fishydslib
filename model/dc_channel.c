/**
 * @file dc_channel.c
 * @brief Channel model implementation
 */

#include "dc_channel.h"
#include "json/dc_json.h"
#include "json/dc_json_model.h"
#include <string.h>

static void dc_channel_free_forum_tags(dc_vec_t* tags) {
    if (!tags) return;
    for (size_t i = 0; i < dc_vec_length(tags); i++) {
        dc_channel_forum_tag_t* tag = (dc_channel_forum_tag_t*)dc_vec_at(tags, i);
        if (!tag) continue;
        dc_string_free(&tag->name);
        dc_optional_string_free(&tag->emoji_name);
    }
    dc_vec_free(tags);
}

dc_status_t dc_channel_thread_member_init(dc_channel_thread_member_t* member) {
    if (!member) return DC_ERROR_NULL_POINTER;
    memset(member, 0, sizeof(*member));
    return dc_string_init(&member->join_timestamp);
}

void dc_channel_thread_member_free(dc_channel_thread_member_t* member) {
    if (!member) return;
    dc_string_free(&member->join_timestamp);
    memset(member, 0, sizeof(*member));
}

dc_status_t dc_channel_init(dc_channel_t* channel) {
    if (!channel) return DC_ERROR_NULL_POINTER;
    memset(channel, 0, sizeof(*channel));
    dc_status_t st = dc_string_init(&channel->name);
    if (st != DC_OK) return st;
    st = dc_string_init(&channel->topic);
    if (st != DC_OK) return st;
    st = dc_string_init(&channel->icon);
    if (st != DC_OK) return st;
    st = dc_string_init(&channel->last_pin_timestamp);
    if (st != DC_OK) return st;
    st = dc_string_init(&channel->rtc_region);
    if (st != DC_OK) return st;
    st = dc_vec_init(&channel->available_tags, sizeof(dc_channel_forum_tag_t));
    if (st != DC_OK) return st;
    st = dc_vec_init(&channel->applied_tags, sizeof(dc_snowflake_t));
    if (st != DC_OK) return st;
    st = dc_optional_string_init(&channel->default_reaction_emoji.emoji_name);
    if (st != DC_OK) return st;
    st = dc_string_init(&channel->thread_metadata.archive_timestamp);
    if (st != DC_OK) return st;
    st = dc_nullable_string_init(&channel->thread_metadata.create_timestamp);
    if (st != DC_OK) return st;
    st = dc_string_init(&channel->thread_member.join_timestamp);
    if (st != DC_OK) return st;
    return DC_OK;
}

void dc_channel_free(dc_channel_t* channel) {
    if (!channel) return;
    dc_string_free(&channel->name);
    dc_string_free(&channel->topic);
    dc_string_free(&channel->icon);
    dc_string_free(&channel->last_pin_timestamp);
    dc_string_free(&channel->rtc_region);
    dc_channel_free_forum_tags(&channel->available_tags);
    dc_vec_free(&channel->applied_tags);
    dc_optional_string_free(&channel->default_reaction_emoji.emoji_name);
    dc_string_free(&channel->thread_metadata.archive_timestamp);
    dc_nullable_string_free(&channel->thread_metadata.create_timestamp);
    dc_string_free(&channel->thread_member.join_timestamp);
    memset(channel, 0, sizeof(*channel));
}

dc_status_t dc_channel_from_json(const char* json_data, dc_channel_t* channel) {
    if (!json_data || !channel) return DC_ERROR_NULL_POINTER;
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(json_data, &doc);
    if (st != DC_OK) return st;

    dc_channel_t tmp;
    st = dc_channel_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    st = dc_json_model_channel_from_val(doc.root, &tmp);
    dc_json_doc_free(&doc);
    if (st != DC_OK) {
        dc_channel_free(&tmp);
        return st;
    }
    *channel = tmp;
    return DC_OK;
}

dc_status_t dc_channel_to_json(const dc_channel_t* channel, dc_string_t* json_result) {
    if (!channel || !json_result) return DC_ERROR_NULL_POINTER;
    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;

    st = dc_json_model_channel_to_mut(&doc, doc.root, channel);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }

    st = dc_json_mut_doc_serialize(&doc, json_result);
    dc_json_mut_doc_free(&doc);
    return st;
}

dc_status_t dc_channel_list_init(dc_channel_list_t* list) {
    if (!list) return DC_ERROR_NULL_POINTER;
    memset(list, 0, sizeof(*list));
    return dc_vec_init(&list->items, sizeof(dc_channel_t));
}

void dc_channel_list_free(dc_channel_list_t* list) {
    if (!list) return;
    for (size_t i = 0; i < dc_vec_length(&list->items); i++) {
        dc_channel_t* channel = (dc_channel_t*)dc_vec_at(&list->items, i);
        if (channel) dc_channel_free(channel);
    }
    dc_vec_free(&list->items);
    memset(list, 0, sizeof(*list));
}
