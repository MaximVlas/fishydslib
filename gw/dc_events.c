/**
 * @file dc_events.c
 * @brief Gateway event parsing helpers
 */

#include "dc_events.h"
#include "json/dc_json.h"
#include "json/dc_json_model.h"
#include "core/dc_snowflake.h"
#include "model/dc_guild.h"
#include "model/dc_message.h"
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
    if (strcmp(name, "READY") == 0) return DC_GATEWAY_EVENT_READY;
    if (strcmp(name, "GUILD_CREATE") == 0) return DC_GATEWAY_EVENT_GUILD_CREATE;
    if (strcmp(name, "MESSAGE_CREATE") == 0) return DC_GATEWAY_EVENT_MESSAGE_CREATE;
    return DC_GATEWAY_EVENT_UNKNOWN;
}

int dc_gateway_event_is_thread_event(const char* name) {
    dc_gateway_event_kind_t kind = dc_gateway_event_kind_from_name(name);
    return (kind == DC_GATEWAY_EVENT_THREAD_CREATE ||
            kind == DC_GATEWAY_EVENT_THREAD_UPDATE ||
            kind == DC_GATEWAY_EVENT_THREAD_DELETE ||
            kind == DC_GATEWAY_EVENT_THREAD_LIST_SYNC ||
            kind == DC_GATEWAY_EVENT_THREAD_MEMBER_UPDATE ||
            kind == DC_GATEWAY_EVENT_THREAD_MEMBERS_UPDATE);
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

    const char* str = NULL;
    if (yyjson_is_str(field)) {
        str = yyjson_get_str(field);
    } else if (yyjson_is_obj(field)) {
        /* Some gateway fields (e.g. READY.application) are objects containing an id. */
        yyjson_val* id_field = yyjson_obj_get(field, "id");
        if (!id_field || yyjson_is_null(id_field)) return DC_ERROR_INVALID_FORMAT;
        if (!yyjson_is_str(id_field)) return DC_ERROR_INVALID_FORMAT;
        str = yyjson_get_str(id_field);
    } else {
        return DC_ERROR_INVALID_FORMAT;
    }
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
        if (member) dc_channel_thread_member_free(member);
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
        if (channel) dc_channel_free(channel);
    }
    for (size_t i = 0; i < dc_vec_length(&sync->members); i++) {
        dc_channel_thread_member_t* member = (dc_channel_thread_member_t*)dc_vec_at(&sync->members, i);
        if (member) dc_channel_thread_member_free(member);
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

/* READY event */

dc_status_t dc_gateway_ready_init(dc_gateway_ready_t* ready) {
    if (!ready) return DC_ERROR_NULL_POINTER;
    memset(ready, 0, sizeof(*ready));

    dc_status_t st = dc_user_init(&ready->user);
    if (st != DC_OK) return st;

    st = dc_vec_init(&ready->guilds, sizeof(dc_gateway_unavailable_guild_t));
    if (st != DC_OK) {
        dc_user_free(&ready->user);
        return st;
    }

    st = dc_string_init(&ready->session_id);
    if (st != DC_OK) goto fail;

    st = dc_string_init(&ready->resume_gateway_url);
    if (st != DC_OK) goto fail;

    st = dc_vec_init(&ready->shard, sizeof(int));
    if (st != DC_OK) goto fail;

    return DC_OK;

fail:
    dc_gateway_ready_free(ready);
    return st;
}

void dc_gateway_ready_free(dc_gateway_ready_t* ready) {
    if (!ready) return;
    dc_user_free(&ready->user);
    dc_vec_free(&ready->guilds);

    dc_string_free(&ready->session_id);
    dc_string_free(&ready->resume_gateway_url);
    dc_vec_free(&ready->shard);
    memset(ready, 0, sizeof(*ready));
}

dc_status_t dc_gateway_event_parse_ready(const char* event_data, dc_gateway_ready_t* ready) {
    if (!event_data || !ready) return DC_ERROR_NULL_POINTER;
    
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(event_data, &doc);
    if (st != DC_OK) return st;

    dc_gateway_ready_t tmp;
    st = dc_gateway_ready_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    // Version
    int64_t v_i64 = 0;
    st = dc_json_get_int64(doc.root, "v", &v_i64);
    if (st != DC_OK) goto fail;
    tmp.v = (int)v_i64;

    // User
    yyjson_val* user_val = yyjson_obj_get(doc.root, "user");
    if (user_val) {
        st = dc_json_model_user_from_val(user_val, &tmp.user);
        if (st != DC_OK) goto fail;
    }

    // Guilds (unavailable partials)
    yyjson_val* guilds_val = yyjson_obj_get(doc.root, "guilds");
    if (guilds_val && yyjson_is_arr(guilds_val)) {
        size_t idx, max;
        yyjson_val* g_val;
        yyjson_arr_foreach(guilds_val, idx, max, g_val) {
            if (!yyjson_is_obj(g_val)) {
                st = DC_ERROR_INVALID_FORMAT;
                goto fail;
            }

            dc_gateway_unavailable_guild_t g = {0};
            uint64_t id_u64 = 0;
            st = dc_json_get_snowflake(g_val, "id", &id_u64);
            if (st != DC_OK) goto fail;
            g.id = (dc_snowflake_t)id_u64;

            st = dc_json_get_bool_opt(g_val, "unavailable", &g.unavailable, 1);
            if (st != DC_OK) goto fail;

            st = dc_vec_push(&tmp.guilds, &g);
            if (st != DC_OK) goto fail;
        }
    }

    // Session ID
    const char* sess = NULL;
    st = dc_json_get_string(doc.root, "session_id", &sess);
    if (st != DC_OK) goto fail;
    st = dc_string_set_cstr(&tmp.session_id, sess);
    if (st != DC_OK) goto fail;

    // Resume URL
    const char* resume = NULL;
    st = dc_json_get_string(doc.root, "resume_gateway_url", &resume);
    if (st != DC_OK) goto fail;
    st = dc_string_set_cstr(&tmp.resume_gateway_url, resume);
    if (st != DC_OK) goto fail;

    // Shard
    yyjson_val* shard_val = yyjson_obj_get(doc.root, "shard");
    if (shard_val && yyjson_is_arr(shard_val)) {
        size_t idx, max;
        yyjson_val* s_val;
        yyjson_arr_foreach(shard_val, idx, max, s_val) {
            if (yyjson_is_int(s_val)) {
                int s_int = yyjson_get_int(s_val); // May need safe cast/check
                st = dc_vec_push(&tmp.shard, &s_int);
                if (st != DC_OK) goto fail;
            }
        }
    }

    // Application ID
    st = dc_gateway_parse_optional_snowflake(doc.root, "application", &tmp.application_id);
    if (st != DC_OK) goto fail;

    dc_json_doc_free(&doc);
    *ready = tmp;
    return DC_OK;

fail:
    dc_json_doc_free(&doc);
    dc_gateway_ready_free(&tmp);
    return st;
}

/* GUILD_CREATE event */

dc_status_t dc_gateway_guild_create_init(dc_gateway_guild_create_t* guild) {
    if (!guild) return DC_ERROR_NULL_POINTER;
    memset(guild, 0, sizeof(*guild));

    dc_status_t st = dc_guild_init(&guild->guild);
    if (st != DC_OK) return st;
    
    st = dc_string_init(&guild->joined_at);
    if (st != DC_OK) goto fail;

    st = dc_vec_init(&guild->members, sizeof(dc_guild_member_t));
    if (st != DC_OK) goto fail;

    st = dc_vec_init(&guild->channels, sizeof(dc_channel_t));
    if (st != DC_OK) goto fail;

    st = dc_vec_init(&guild->threads, sizeof(dc_channel_t));
    if (st != DC_OK) goto fail;
    
    st = dc_vec_init(&guild->voice_states, sizeof(dc_voice_state_t));
    if (st != DC_OK) goto fail;

    st = dc_vec_init(&guild->presences, sizeof(dc_presence_t));
    if (st != DC_OK) goto fail;

    st = dc_vec_init(&guild->stage_instances, sizeof(int)); // TODO: stage instance type
    if (st != DC_OK) goto fail;

    st = dc_vec_init(&guild->guild_scheduled_events, sizeof(int)); // TODO: scheduled event type
    if (st != DC_OK) goto fail;

    return DC_OK;

fail:
    dc_gateway_guild_create_free(guild);
    return st;
}

void dc_gateway_guild_create_free(dc_gateway_guild_create_t* guild) {
    if (!guild) return;
    dc_guild_free(&guild->guild);
    dc_string_free(&guild->joined_at);
    
    for (size_t i = 0; i < dc_vec_length(&guild->members); i++) {
        dc_guild_member_t* m = (dc_guild_member_t*)dc_vec_at(&guild->members, i);
        if (m) dc_guild_member_free(m);
    }
    dc_vec_free(&guild->members);

    for (size_t i = 0; i < dc_vec_length(&guild->channels); i++) {
        dc_channel_t* c = (dc_channel_t*)dc_vec_at(&guild->channels, i);
        if (c) dc_channel_free(c);
    }
    dc_vec_free(&guild->channels);

    for (size_t i = 0; i < dc_vec_length(&guild->threads); i++) {
        dc_channel_t* t = (dc_channel_t*)dc_vec_at(&guild->threads, i);
        if (t) dc_channel_free(t);
    }
    dc_vec_free(&guild->threads);
    
    for (size_t i = 0; i < dc_vec_length(&guild->voice_states); i++) {
        dc_voice_state_t* v = (dc_voice_state_t*)dc_vec_at(&guild->voice_states, i);
        if (v) dc_voice_state_free(v);
    }
    dc_vec_free(&guild->voice_states);

    for (size_t i = 0; i < dc_vec_length(&guild->presences); i++) {
        dc_presence_t* p = (dc_presence_t*)dc_vec_at(&guild->presences, i);
        if (p) dc_presence_free(p);
    }
    dc_vec_free(&guild->presences);

    // Placeholders - no element cleanup needed
    dc_vec_free(&guild->stage_instances);
    dc_vec_free(&guild->guild_scheduled_events);

    memset(guild, 0, sizeof(*guild));
}

dc_status_t dc_gateway_event_parse_guild_create(const char* event_data, dc_gateway_guild_create_t* guild) {
    if (!event_data || !guild) return DC_ERROR_NULL_POINTER;

    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(event_data, &doc);
    if (st != DC_OK) return st;

    dc_gateway_guild_create_t tmp;
    st = dc_gateway_guild_create_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    // 1. Parse base guild object
    st = dc_json_model_guild_from_val(doc.root, &tmp.guild);
    if (st != DC_OK) goto fail;

    // 2. Parse extra fields
    const char* joined_at = "";
    st = dc_json_get_string_opt(doc.root, "joined_at", &joined_at, "");
    if (st != DC_OK) goto fail;
    st = dc_string_set_cstr(&tmp.joined_at, joined_at);
    if (st != DC_OK) goto fail;

    st = dc_json_get_bool_opt(doc.root, "large", &tmp.large, 0);
    if (st != DC_OK) goto fail;
    
    st = dc_json_get_bool_opt(doc.root, "unavailable", &tmp.unavailable, 0);
    if (st != DC_OK) goto fail;

    int64_t member_count = 0;
    st = dc_json_get_int64_opt(doc.root, "member_count", &member_count, 0);
    if (st != DC_OK) goto fail;
    tmp.member_count = (int)member_count;

    // 3. Members
    yyjson_val* members = yyjson_obj_get(doc.root, "members");
    if (members && yyjson_is_arr(members)) {
        // We reuse the existing static helper if strictly needed, but it is static...
        // dc_gateway_parse_thread_member_array parses thread members. 
        // Guild members are different, needsto iterate manually here.
        size_t idx, max;
        yyjson_val* m_val;
        yyjson_arr_foreach(members, idx, max, m_val) {
            dc_guild_member_t member;
            st = dc_guild_member_init(&member);
            if (st != DC_OK) goto fail;

            st = dc_json_model_guild_member_from_val(m_val, &member);
            if (st != DC_OK) {
                dc_guild_member_free(&member);
                goto fail;
            }
            st = dc_vec_push(&tmp.members, &member);
            if (st != DC_OK) {
                dc_guild_member_free(&member);
                goto fail;
            }
        }
    }

    // 4. Channels
    yyjson_val* channels = yyjson_obj_get(doc.root, "channels");
    if (channels && yyjson_is_arr(channels)) {
        size_t idx, max;
        yyjson_val* c_val;
        yyjson_arr_foreach(channels, idx, max, c_val) {
            dc_channel_t chan;
            st = dc_channel_init(&chan);
            if (st != DC_OK) goto fail;

            st = dc_json_model_channel_from_val(c_val, &chan);
            if (st != DC_OK) {
                dc_channel_free(&chan);
                goto fail;
            }
            st = dc_vec_push(&tmp.channels, &chan);
            if (st != DC_OK) {
                dc_channel_free(&chan);
                goto fail;
            }
        }
    }

    // 5. Threads
    yyjson_val* threads = yyjson_obj_get(doc.root, "threads");
    if (threads && yyjson_is_arr(threads)) {
        size_t idx, max;
        yyjson_val* t_val;
        yyjson_arr_foreach(threads, idx, max, t_val) {
            dc_channel_t thread;
            st = dc_channel_init(&thread);
            if (st != DC_OK) goto fail;

            st = dc_json_model_channel_from_val(t_val, &thread);
             if (st != DC_OK) {
                dc_channel_free(&thread);
                goto fail;
            }
            st = dc_vec_push(&tmp.threads, &thread);
            if (st != DC_OK) {
                dc_channel_free(&thread);
                goto fail;
            }
        }
    }

    // 6. Voice states
    yyjson_val* voice_states = yyjson_obj_get(doc.root, "voice_states");
    if (voice_states && yyjson_is_arr(voice_states)) {
        size_t idx, max;
        yyjson_val* vs_val;
        yyjson_arr_foreach(voice_states, idx, max, vs_val) {
            dc_voice_state_t vs;
            st = dc_voice_state_init(&vs);
            if (st != DC_OK) goto fail;

            st = dc_json_model_voice_state_from_val(vs_val, &vs);
            if (st != DC_OK) {
                dc_voice_state_free(&vs);
                goto fail;
            }
            st = dc_vec_push(&tmp.voice_states, &vs);
            if (st != DC_OK) {
                dc_voice_state_free(&vs);
                goto fail;
            }
        }
    }

    // 7. Presences
    yyjson_val* presences = yyjson_obj_get(doc.root, "presences");
    if (presences && yyjson_is_arr(presences)) {
        size_t idx, max;
        yyjson_val* p_val;
        yyjson_arr_foreach(presences, idx, max, p_val) {
            dc_presence_t presence;
            st = dc_presence_init(&presence);
            if (st != DC_OK) goto fail;

            st = dc_json_model_presence_from_val(p_val, &presence);
            if (st != DC_OK) {
                dc_presence_free(&presence);
                goto fail;
            }
            st = dc_vec_push(&tmp.presences, &presence);
            if (st != DC_OK) {
                dc_presence_free(&presence);
                goto fail;
            }
        }
    }

    dc_json_doc_free(&doc);
    *guild = tmp;
    return DC_OK;

fail:
    dc_json_doc_free(&doc);
    dc_gateway_guild_create_free(&tmp);
    return st;
}

/* MESSAGE_CREATE event */

dc_status_t dc_gateway_event_parse_message_create(const char* event_data, dc_message_t* message) {
    if (!event_data || !message) return DC_ERROR_NULL_POINTER;

    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(event_data, &doc);
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

/* MESSAGE_CREATE full (with gateway-specific fields) */

dc_status_t dc_gateway_message_create_init(dc_gateway_message_create_t* msg) {
    if (!msg) return DC_ERROR_NULL_POINTER;
    memset(msg, 0, sizeof(*msg));

    dc_status_t st = dc_message_init(&msg->message);
    if (st != DC_OK) return st;

    st = dc_guild_member_init(&msg->member);
    if (st != DC_OK) {
        dc_message_free(&msg->message);
        return st;
    }

    return DC_OK;
}

void dc_gateway_message_create_free(dc_gateway_message_create_t* msg) {
    if (!msg) return;
    dc_message_free(&msg->message);
    if (msg->has_member) {
        dc_guild_member_free(&msg->member);
    }
    memset(msg, 0, sizeof(*msg));
}

dc_status_t dc_gateway_event_parse_message_create_full(const char* event_data,
                                                        dc_gateway_message_create_t* msg) {
    if (!event_data || !msg) return DC_ERROR_NULL_POINTER;

    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(event_data, &doc);
    if (st != DC_OK) return st;

    dc_gateway_message_create_t tmp;
    st = dc_gateway_message_create_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    // Parse core message
    st = dc_json_model_message_from_val(doc.root, &tmp.message);
    if (st != DC_OK) goto fail;

    // Parse guild_id (optional, gateway-specific)
    st = dc_gateway_parse_optional_snowflake(doc.root, "guild_id", &tmp.guild_id);
    if (st != DC_OK) goto fail;

    // Parse member (optional partial guild member, gateway-specific)
    yyjson_val* member_val = yyjson_obj_get(doc.root, "member");
    if (member_val && yyjson_is_obj(member_val)) {
        st = dc_json_model_guild_member_from_val(member_val, &tmp.member);
        if (st != DC_OK) goto fail;
        tmp.has_member = 1;
    }

    dc_json_doc_free(&doc);
    *msg = tmp;
    return DC_OK;

fail:
    dc_json_doc_free(&doc);
    dc_gateway_message_create_free(&tmp);
    return st;
}
