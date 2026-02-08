/**
 * @file dc_events.c
 * @brief Gateway event parsing helpers
 */

#include "dc_events.h"
#include "json/dc_json.h"
#include "json/dc_json_model.h"
#include "core/dc_snowflake.h"
#include <string.h>
#include <yyjson.h>

dc_gateway_event_kind_t dc_gateway_event_kind_from_name(const char* name) {
    if (!name) return DC_GATEWAY_EVENT_UNKNOWN;
    if (strcmp(name, "THREAD_CREATE") == 0) return DC_GATEWAY_EVENT_THREAD_CREATE;
    if (strcmp(name, "THREAD_UPDATE") == 0) return DC_GATEWAY_EVENT_THREAD_UPDATE;
    if (strcmp(name, "THREAD_DELETE") == 0) return DC_GATEWAY_EVENT_THREAD_DELETE;
    if (strcmp(name, "THREAD_LIST_SYNC") == 0) return DC_GATEWAY_EVENT_THREAD_LIST_SYNC;
    if (strcmp(name, "THREAD_MEMBER_UPDATE") == 0) return DC_GATEWAY_EVENT_THREAD_MEMBER_UPDATE;
    if (strcmp(name, "THREAD_MEMBERS_UPDATE") == 0) return DC_GATEWAY_EVENT_THREAD_MEMBERS_UPDATE;
    return DC_GATEWAY_EVENT_UNKNOWN;
}

int dc_gateway_event_is_thread_event(const char* name) {
    return dc_gateway_event_kind_from_name(name) != DC_GATEWAY_EVENT_UNKNOWN;
}

static dc_status_t dc_gateway_parse_optional_snowflake(yyjson_val* obj,
                                                       const char* key,
                                                       dc_optional_snowflake_t* out) {
    if (!obj || !key || !out) return DC_ERROR_NULL_POINTER;
    yyjson_val* field = yyjson_obj_get(obj, key);
    if (!field || yyjson_is_null(field)) {
        out->is_set = 0;
        out->value = 0;
        return DC_OK;
    }
    if (!yyjson_is_str(field)) return DC_ERROR_INVALID_FORMAT;
    const char* str = yyjson_get_str(field);
    if (!str) return DC_ERROR_INVALID_FORMAT;
    dc_snowflake_t sf = 0;
    dc_status_t st = dc_snowflake_from_string(str, &sf);
    if (st != DC_OK) return st;
    out->is_set = 1;
    out->value = sf;
    return DC_OK;
}

static dc_status_t dc_gateway_parse_snowflake_array(yyjson_val* arr, dc_vec_t* out) {
    if (!arr || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_arr(arr)) return DC_ERROR_INVALID_FORMAT;

    size_t idx, max;
    yyjson_val* val;
    yyjson_arr_foreach(arr, idx, max, val) {
        if (!yyjson_is_str(val)) return DC_ERROR_INVALID_FORMAT;
        const char* str = yyjson_get_str(val);
        if (!str) return DC_ERROR_INVALID_FORMAT;
        dc_snowflake_t sf = 0;
        dc_status_t st = dc_snowflake_from_string(str, &sf);
        if (st != DC_OK) return st;
        st = dc_vec_push(out, &sf);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

static dc_status_t dc_gateway_parse_thread_member_array(yyjson_val* arr, dc_vec_t* out) {
    if (!arr || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_arr(arr)) return DC_ERROR_INVALID_FORMAT;

    size_t idx, max;
    yyjson_val* val;
    yyjson_arr_foreach(arr, idx, max, val) {
        dc_channel_thread_member_t member;
        dc_status_t st = dc_channel_thread_member_init(&member);
        if (st != DC_OK) return st;
        st = dc_json_model_thread_member_from_val(val, &member);
        if (st != DC_OK) {
            dc_channel_thread_member_free(&member);
            return st;
        }
        st = dc_vec_push(out, &member);
        if (st != DC_OK) {
            dc_channel_thread_member_free(&member);
            return st;
        }
    }
    return DC_OK;
}

static dc_status_t dc_gateway_parse_thread_channel_array(yyjson_val* arr, dc_vec_t* out) {
    if (!arr || !out) return DC_ERROR_NULL_POINTER;
    if (!yyjson_is_arr(arr)) return DC_ERROR_INVALID_FORMAT;

    size_t idx, max;
    yyjson_val* val;
    yyjson_arr_foreach(arr, idx, max, val) {
        dc_channel_t channel;
        dc_status_t st = dc_channel_init(&channel);
        if (st != DC_OK) return st;
        st = dc_json_model_channel_from_val(val, &channel);
        if (st != DC_OK) {
            dc_channel_free(&channel);
            return st;
        }
        st = dc_vec_push(out, &channel);
        if (st != DC_OK) {
            dc_channel_free(&channel);
            return st;
        }
    }
    return DC_OK;
}

dc_status_t dc_gateway_event_parse_thread_channel(const char* event_data, dc_channel_t* channel) {
    if (!event_data || !channel) return DC_ERROR_NULL_POINTER;
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(event_data, &doc);
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

dc_status_t dc_gateway_event_parse_thread_member(const char* event_data,
                                                 dc_channel_thread_member_t* member) {
    if (!event_data || !member) return DC_ERROR_NULL_POINTER;
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(event_data, &doc);
    if (st != DC_OK) return st;

    dc_channel_thread_member_t tmp;
    st = dc_channel_thread_member_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    st = dc_json_model_thread_member_from_val(doc.root, &tmp);
    dc_json_doc_free(&doc);
    if (st != DC_OK) {
        dc_channel_thread_member_free(&tmp);
        return st;
    }
    *member = tmp;
    return DC_OK;
}

dc_status_t dc_gateway_thread_members_update_init(dc_gateway_thread_members_update_t* update) {
    if (!update) return DC_ERROR_NULL_POINTER;
    memset(update, 0, sizeof(*update));
    dc_status_t st = dc_vec_init(&update->members, sizeof(dc_channel_thread_member_t));
    if (st != DC_OK) return st;
    st = dc_vec_init(&update->removed_member_ids, sizeof(dc_snowflake_t));
    if (st != DC_OK) {
        dc_vec_free(&update->members);
        return st;
    }
    return DC_OK;
}

void dc_gateway_thread_members_update_free(dc_gateway_thread_members_update_t* update) {
    if (!update) return;
    for (size_t i = 0; i < dc_vec_length(&update->members); i++) {
        dc_channel_thread_member_t* member = (dc_channel_thread_member_t*)dc_vec_at(&update->members, i);
        dc_channel_thread_member_free(member);
    }
    dc_vec_free(&update->members);
    dc_vec_free(&update->removed_member_ids);
    memset(update, 0, sizeof(*update));
}

dc_status_t dc_gateway_event_parse_thread_members_update(const char* event_data,
                                                         dc_gateway_thread_members_update_t* update) {
    if (!event_data || !update) return DC_ERROR_NULL_POINTER;
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(event_data, &doc);
    if (st != DC_OK) return st;

    dc_gateway_thread_members_update_t tmp;
    st = dc_gateway_thread_members_update_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    st = dc_gateway_parse_optional_snowflake(doc.root, "guild_id", &tmp.guild_id);
    if (st != DC_OK) goto fail;
    st = dc_gateway_parse_optional_snowflake(doc.root, "id", &tmp.thread_id);
    if (st != DC_OK) goto fail;

    yyjson_val* members = yyjson_obj_get(doc.root, "members");
    if (members && !yyjson_is_null(members)) {
        st = dc_gateway_parse_thread_member_array(members, &tmp.members);
        if (st != DC_OK) goto fail;
    } else {
        yyjson_val* added_members = yyjson_obj_get(doc.root, "added_members");
        if (added_members && !yyjson_is_null(added_members)) {
            st = dc_gateway_parse_thread_member_array(added_members, &tmp.members);
            if (st != DC_OK) goto fail;
        }
    }

    yyjson_val* removed = yyjson_obj_get(doc.root, "removed_member_ids");
    if (removed && !yyjson_is_null(removed)) {
        st = dc_gateway_parse_snowflake_array(removed, &tmp.removed_member_ids);
        if (st != DC_OK) goto fail;
    }

    dc_json_doc_free(&doc);
    *update = tmp;
    return DC_OK;

fail:
    dc_json_doc_free(&doc);
    dc_gateway_thread_members_update_free(&tmp);
    return st;
}

dc_status_t dc_gateway_thread_list_sync_init(dc_gateway_thread_list_sync_t* sync) {
    if (!sync) return DC_ERROR_NULL_POINTER;
    memset(sync, 0, sizeof(*sync));
    dc_status_t st = dc_vec_init(&sync->channel_ids, sizeof(dc_snowflake_t));
    if (st != DC_OK) return st;
    st = dc_vec_init(&sync->threads, sizeof(dc_channel_t));
    if (st != DC_OK) {
        dc_vec_free(&sync->channel_ids);
        return st;
    }
    st = dc_vec_init(&sync->members, sizeof(dc_channel_thread_member_t));
    if (st != DC_OK) {
        dc_vec_free(&sync->threads);
        dc_vec_free(&sync->channel_ids);
        return st;
    }
    return DC_OK;
}

void dc_gateway_thread_list_sync_free(dc_gateway_thread_list_sync_t* sync) {
    if (!sync) return;
    for (size_t i = 0; i < dc_vec_length(&sync->threads); i++) {
        dc_channel_t* channel = (dc_channel_t*)dc_vec_at(&sync->threads, i);
        dc_channel_free(channel);
    }
    for (size_t i = 0; i < dc_vec_length(&sync->members); i++) {
        dc_channel_thread_member_t* member = (dc_channel_thread_member_t*)dc_vec_at(&sync->members, i);
        dc_channel_thread_member_free(member);
    }
    dc_vec_free(&sync->threads);
    dc_vec_free(&sync->members);
    dc_vec_free(&sync->channel_ids);
    memset(sync, 0, sizeof(*sync));
}

dc_status_t dc_gateway_event_parse_thread_list_sync(const char* event_data,
                                                    dc_gateway_thread_list_sync_t* sync) {
    if (!event_data || !sync) return DC_ERROR_NULL_POINTER;
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(event_data, &doc);
    if (st != DC_OK) return st;

    dc_gateway_thread_list_sync_t tmp;
    st = dc_gateway_thread_list_sync_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    st = dc_gateway_parse_optional_snowflake(doc.root, "guild_id", &tmp.guild_id);
    if (st != DC_OK) goto fail;

    yyjson_val* channels = yyjson_obj_get(doc.root, "channel_ids");
    if (channels && !yyjson_is_null(channels)) {
        st = dc_gateway_parse_snowflake_array(channels, &tmp.channel_ids);
        if (st != DC_OK) goto fail;
    }

    yyjson_val* threads = yyjson_obj_get(doc.root, "threads");
    if (threads && !yyjson_is_null(threads)) {
        st = dc_gateway_parse_thread_channel_array(threads, &tmp.threads);
        if (st != DC_OK) goto fail;
    }

    yyjson_val* members = yyjson_obj_get(doc.root, "members");
    if (members && !yyjson_is_null(members)) {
        st = dc_gateway_parse_thread_member_array(members, &tmp.members);
        if (st != DC_OK) goto fail;
    }

    dc_json_doc_free(&doc);
    *sync = tmp;
    return DC_OK;

fail:
    dc_json_doc_free(&doc);
    dc_gateway_thread_list_sync_free(&tmp);
    return st;
}
