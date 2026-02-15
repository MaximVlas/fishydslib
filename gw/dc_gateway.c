/**
 * @file dc_gateway.c
 * @brief Gateway client implementation (v10, JSON encoding)
 */

#include "dc_gateway.h"
#include "core/dc_alloc.h"
#include "core/dc_vec.h"
#include "http/dc_http_compliance.h"
#include "json/dc_json.h"
#include <libwebsockets.h>
#include <yyjson.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#define DC_GATEWAY_API_VERSION 10
#define DC_GATEWAY_ENCODING "json"
#define DC_GATEWAY_COMPRESS_QUERY "compress=zlib-stream"
#define DC_GATEWAY_SEND_LIMIT 120u
#define DC_GATEWAY_SEND_WINDOW_MS 60000u
#define DC_GATEWAY_IDENTIFY_INTERVAL_MS 5000u
#define DC_GATEWAY_INVALID_SESSION_BACKOFF_MIN_MS 1000u
#define DC_GATEWAY_INVALID_SESSION_BACKOFF_MAX_MS 5000u
#define DC_GATEWAY_ZLIB_SUFFIX_LEN 4u
#define DC_GATEWAY_RX_INITIAL_CAP 8192u
#define DC_GATEWAY_COMPRESSED_INITIAL_CAP 8192u
#define DC_GATEWAY_EVENT_INITIAL_CAP 4096u
#define DC_GATEWAY_TX_INITIAL_CAP 4096u
#define DC_GATEWAY_RECONNECT_MIN_MS 1000u
#define DC_GATEWAY_RECONNECT_MAX_MS 30000u

typedef struct {
    dc_string_t payload;
    uint64_t due_ms;
    int urgent;
    int opcode;
} dc_gateway_outgoing_t;

struct dc_gateway_client {
    dc_string_t token;
    dc_string_t user_agent;
    uint32_t intents;
    uint32_t shard_id;
    uint32_t shard_count;
    uint32_t large_threshold;
    dc_gateway_event_callback_t event_callback;
    dc_gateway_state_callback_t state_callback;
    void* user_data;
    uint32_t heartbeat_timeout_ms;
    uint32_t connect_timeout_ms;
    int enable_compression;
    int enable_payload_compression;
    dc_gateway_state_t state;

    struct lws_context* context;
    struct lws* wsi;

    dc_string_t base_url;
    dc_string_t connect_url;
    dc_string_t resume_url;
    dc_string_t session_id;

    uint32_t heartbeat_interval_ms;
    uint64_t next_heartbeat_ms;
    uint64_t last_heartbeat_sent_ms;
    uint64_t last_heartbeat_ack_ms;
    int awaiting_heartbeat_ack;

    int has_seq;
    int64_t last_seq;
    int has_dispatch_seq;
    int64_t last_dispatch_seq;

    int should_resume;
    int reconnect_requested;
    int manual_disconnect;
    uint64_t reconnect_at_ms;
    uint32_t reconnect_backoff_ms;

    uint64_t send_window_start_ms;
    uint32_t send_count;
    uint64_t send_block_until_ms;
    uint64_t last_identify_ms;
    uint64_t identify_due_ms;
    uint64_t connect_deadline_ms;

    dc_vec_t outbox;
    dc_string_t rx_buf;
    dc_string_t compressed_buf;
    dc_string_t event_buf;
    unsigned char* tx_buf;
    size_t tx_buf_cap;
    z_stream zstrm;
    int zinit;

    dc_status_t last_error;
};

static void dc_gateway_outbox_clear(dc_gateway_client_t* client);

static uint64_t dc_gateway_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void dc_gateway_set_state(dc_gateway_client_t* client, dc_gateway_state_t state) {
    if (!client) return;
    client->state = state;
    if (client->state_callback) {
        client->state_callback(state, client->user_data);
    }
}

static void dc_gateway_clear_session(dc_gateway_client_t* client) {
    if (!client) return;
    client->should_resume = 0;
    client->has_seq = 0;
    client->last_seq = 0;
    client->has_dispatch_seq = 0;
    client->last_dispatch_seq = 0;
    dc_string_clear(&client->session_id);
    dc_string_clear(&client->resume_url);
}

static int dc_gateway_close_requires_reidentify(int code) {
    switch (code) {
        case DC_GATEWAY_CLOSE_INVALID_SEQ:
        case DC_GATEWAY_CLOSE_SESSION_TIMED_OUT:
            return 1;
        default:
            return 0;
    }
}

static void dc_gateway_schedule_reconnect(dc_gateway_client_t* client) {
    if (!client) return;
    uint64_t now = dc_gateway_now_ms();
    if (client->reconnect_backoff_ms == 0) {
        client->reconnect_backoff_ms = DC_GATEWAY_RECONNECT_MIN_MS;
    } else if (client->reconnect_backoff_ms < DC_GATEWAY_RECONNECT_MAX_MS) {
        uint32_t next = client->reconnect_backoff_ms <= DC_GATEWAY_RECONNECT_MAX_MS / 2u
                        ? client->reconnect_backoff_ms * 2u
                        : DC_GATEWAY_RECONNECT_MAX_MS;
        if (next > DC_GATEWAY_RECONNECT_MAX_MS) {
            next = DC_GATEWAY_RECONNECT_MAX_MS;
        }
        client->reconnect_backoff_ms = next;
    }
    uint32_t jitter = client->reconnect_backoff_ms / 5u;
    uint32_t jitter_add = jitter > 0 ? (uint32_t)(rand() % (jitter + 1u)) : 0u;
    uint64_t total = (uint64_t)client->reconnect_backoff_ms + jitter_add;
    if (total > DC_GATEWAY_RECONNECT_MAX_MS) {
        total = DC_GATEWAY_RECONNECT_MAX_MS;
    }
    client->reconnect_at_ms = now + total;
    client->reconnect_requested = 1;
    client->awaiting_heartbeat_ack = 0;
    client->heartbeat_interval_ms = 0;
    client->next_heartbeat_ms = 0;
    client->last_heartbeat_sent_ms = 0;
    client->last_heartbeat_ack_ms = 0;
    client->identify_due_ms = 0;
    client->connect_deadline_ms = 0;
    client->send_window_start_ms = 0;
    client->send_count = 0;
    client->send_block_until_ms = 0;
    dc_string_clear(&client->rx_buf);
    dc_string_clear(&client->compressed_buf);
    dc_gateway_outbox_clear(client);
    if (client->zinit) {
        inflateReset(&client->zstrm);
    }
}

static int dc_gateway_get_close_code(struct lws* wsi) {
    int len = lws_get_close_length(wsi);
    if (len < 2) return 0;
    unsigned char* payload = lws_get_close_payload(wsi);
    if (!payload) return 0;
    return (int)((payload[0] << 8) | payload[1]);
}

static void dc_gateway_handle_close(dc_gateway_client_t* client, int code) {
    if (!client) return;
    if (code >= 4000 && !dc_gateway_close_code_should_reconnect(code)) {
        client->reconnect_requested = 0;
        dc_gateway_clear_session(client);
        if (code == DC_GATEWAY_CLOSE_AUTHENTICATION_FAILED) {
            client->last_error = DC_ERROR_UNAUTHORIZED;
        } else if (code == DC_GATEWAY_CLOSE_INVALID_INTENTS ||
                   code == DC_GATEWAY_CLOSE_DISALLOWED_INTENTS) {
            client->last_error = DC_ERROR_INVALID_PARAM;
        } else {
            client->last_error = DC_ERROR_INVALID_STATE;
        }
        client->manual_disconnect = 1;
        return;
    }
    if (client->manual_disconnect) {
        dc_gateway_clear_session(client);
        return;
    }
    if (dc_gateway_close_requires_reidentify(code)) {
        dc_gateway_clear_session(client);
    }
}

static void dc_gateway_outgoing_free(dc_gateway_outgoing_t* msg) {
    if (!msg) return;
    dc_string_free(&msg->payload);
}

static void dc_gateway_outbox_clear(dc_gateway_client_t* client) {
    if (!client) return;
    for (size_t i = 0; i < client->outbox.length; i++) {
        dc_gateway_outgoing_t* msg = (dc_gateway_outgoing_t*)dc_vec_at(&client->outbox, i);
        dc_gateway_outgoing_free(msg);
    }
    dc_vec_clear(&client->outbox);
}

static int dc_gateway_outbox_has_ready(const dc_gateway_client_t* client, uint64_t now_ms) {
    if (!client) return 0;
    for (size_t i = 0; i < client->outbox.length; i++) {
        const dc_gateway_outgoing_t* msg = (const dc_gateway_outgoing_t*)dc_vec_at(&client->outbox, i);
        if (msg && msg->due_ms <= now_ms) return 1;
    }
    return 0;
}

static dc_status_t dc_gateway_outbox_push_at(dc_gateway_client_t* client,
                                             const char* data,
                                             size_t len,
                                             int urgent,
                                             uint64_t due_ms,
                                             int opcode) {
    if (!client || !data) return DC_ERROR_NULL_POINTER;
    if (len > 4096u) return DC_ERROR_INVALID_PARAM;
    dc_gateway_outgoing_t msg;
    dc_status_t st = dc_string_init_from_buffer(&msg.payload, data, len);
    if (st != DC_OK) return st;
    msg.due_ms = due_ms;
    msg.urgent = urgent ? 1 : 0;
    msg.opcode = opcode;
    if (urgent) {
        st = dc_vec_insert(&client->outbox, (size_t)0, &msg);
    } else {
        st = dc_vec_push(&client->outbox, &msg);
    }
    if (st != DC_OK) {
        dc_gateway_outgoing_free(&msg);
        return st;
    }
    return DC_OK;
}

static int dc_gateway_rate_limit_allows_send(dc_gateway_client_t* client, uint64_t now_ms) {
    if (client->send_block_until_ms > now_ms) return 0;
    if (now_ms - client->send_window_start_ms >= DC_GATEWAY_SEND_WINDOW_MS) {
        client->send_window_start_ms = now_ms;
        client->send_count = 0;
        client->send_block_until_ms = 0;
    }
    return 1;
}

static void dc_gateway_rate_limit_commit_send(dc_gateway_client_t* client, uint64_t now_ms) {
    client->send_count++;
    if (client->send_count >= DC_GATEWAY_SEND_LIMIT) {
        client->send_block_until_ms = client->send_window_start_ms + DC_GATEWAY_SEND_WINDOW_MS;
    }
    if (client->send_window_start_ms == 0) {
        client->send_window_start_ms = now_ms;
    }
}

static const char* dc_gateway_os_string(void) {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

static dc_status_t dc_gateway_json_serialize(yyjson_mut_doc* doc, dc_string_t* out) {
    if (!doc || !out) return DC_ERROR_NULL_POINTER;
    size_t json_len = 0;
    char* json = yyjson_mut_write(doc, 0, &json_len);
    if (!json) return DC_ERROR_JSON;
    dc_status_t st = dc_string_set_buffer(out, json, json_len);
    free(json);
    return st;
}

static dc_status_t dc_gateway_build_heartbeat_payload(dc_gateway_client_t* client, dc_string_t* out) {
    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;
    yyjson_mut_val* root = doc.root;
    yyjson_mut_obj_add_int(doc.doc, root, "op", (int64_t)DC_GATEWAY_OP_HEARTBEAT);
    if (client->has_seq) {
        yyjson_mut_obj_add_int(doc.doc, root, "d", client->last_seq);
    } else {
        yyjson_mut_obj_add_null(doc.doc, root, "d");
    }
    st = dc_gateway_json_serialize(doc.doc, out);
    dc_json_mut_doc_free(&doc);
    return st;
}

static dc_status_t dc_gateway_build_identify_payload(dc_gateway_client_t* client, dc_string_t* out) {
    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;
    yyjson_mut_val* root = doc.root;
    yyjson_mut_obj_add_int(doc.doc, root, "op", (int64_t)DC_GATEWAY_OP_IDENTIFY);

    yyjson_mut_val* d = yyjson_mut_obj(doc.doc);
    yyjson_mut_obj_add(root, yyjson_mut_strcpy(doc.doc, "d"), d);

    yyjson_mut_obj_add_strcpy(doc.doc, d, "token", dc_string_cstr(&client->token));
    yyjson_mut_obj_add_int(doc.doc, d, "intents", (int64_t)client->intents);

    yyjson_mut_val* props = yyjson_mut_obj(doc.doc);
    yyjson_mut_obj_add(d, yyjson_mut_strcpy(doc.doc, "properties"), props);
    yyjson_mut_obj_add_strcpy(doc.doc, props, "os", dc_gateway_os_string());
    yyjson_mut_obj_add_strcpy(doc.doc, props, "browser", "fishydslib");
    yyjson_mut_obj_add_strcpy(doc.doc, props, "device", "fishydslib");

    if (client->shard_count > 0) {
        yyjson_mut_val* shard = yyjson_mut_arr(doc.doc);
        yyjson_mut_arr_add_val(shard, yyjson_mut_sint(doc.doc, (int64_t)client->shard_id));
        yyjson_mut_arr_add_val(shard, yyjson_mut_sint(doc.doc, (int64_t)client->shard_count));
        yyjson_mut_obj_add(d, yyjson_mut_strcpy(doc.doc, "shard"), shard);
    }
    if (client->large_threshold > 0) {
        yyjson_mut_obj_add_int(doc.doc, d, "large_threshold", (int64_t)client->large_threshold);
    }
    if (client->enable_payload_compression) {
        yyjson_mut_val* compress = yyjson_mut_true(doc.doc);
        yyjson_mut_obj_add(d, yyjson_mut_strcpy(doc.doc, "compress"), compress);
    }

    st = dc_gateway_json_serialize(doc.doc, out);
    dc_json_mut_doc_free(&doc);
    return st;
}

static dc_status_t dc_gateway_build_resume_payload(dc_gateway_client_t* client, dc_string_t* out) {
    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;
    yyjson_mut_val* root = doc.root;
    yyjson_mut_obj_add_int(doc.doc, root, "op", (int64_t)DC_GATEWAY_OP_RESUME);

    yyjson_mut_val* d = yyjson_mut_obj(doc.doc);
    yyjson_mut_obj_add(root, yyjson_mut_strcpy(doc.doc, "d"), d);

    yyjson_mut_obj_add_strcpy(doc.doc, d, "token", dc_string_cstr(&client->token));
    yyjson_mut_obj_add_strcpy(doc.doc, d, "session_id", dc_string_cstr(&client->session_id));
    yyjson_mut_obj_add_int(doc.doc, d, "seq", client->last_seq);

    st = dc_gateway_json_serialize(doc.doc, out);
    dc_json_mut_doc_free(&doc);
    return st;
}

static dc_status_t dc_gateway_build_presence_payload(const char* status, const char* activity_name,
                                                     int activity_type, dc_string_t* out) {
    if (!status || !out) return DC_ERROR_NULL_POINTER;

    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;
    yyjson_mut_val* root = doc.root;
    yyjson_mut_obj_add_int(doc.doc, root, "op", (int64_t)DC_GATEWAY_OP_PRESENCE_UPDATE);

    yyjson_mut_val* d = yyjson_mut_obj(doc.doc);
    yyjson_mut_obj_add(root, yyjson_mut_strcpy(doc.doc, "d"), d);
    yyjson_mut_obj_add_null(doc.doc, d, "since");
    yyjson_mut_obj_add_strcpy(doc.doc, d, "status", status);
    yyjson_mut_val* afk_val = yyjson_mut_false(doc.doc);
    yyjson_mut_obj_add(d, yyjson_mut_strcpy(doc.doc, "afk"), afk_val);

    yyjson_mut_val* activities = yyjson_mut_arr(doc.doc);
    yyjson_mut_obj_add(d, yyjson_mut_strcpy(doc.doc, "activities"), activities);
    if (activity_name && activity_name[0] != '\0') {
        yyjson_mut_val* act = yyjson_mut_obj(doc.doc);
        yyjson_mut_obj_add_strcpy(doc.doc, act, "name", activity_name);
        yyjson_mut_obj_add_int(doc.doc, act, "type", (int64_t)activity_type);
        yyjson_mut_arr_add_val(activities, act);
    }

    st = dc_gateway_json_serialize(doc.doc, out);
    dc_json_mut_doc_free(&doc);
    return st;
}

static dc_status_t dc_gateway_build_request_guild_members_payload(dc_snowflake_t guild_id,
                                                                  const char* query,
                                                                  uint32_t limit,
                                                                  int presences,
                                                                  const dc_snowflake_t* user_ids,
                                                                  size_t user_id_count,
                                                                  const char* nonce,
                                                                  dc_string_t* out) {
    if (!dc_snowflake_is_valid(guild_id) || !out) return DC_ERROR_INVALID_PARAM;
    if (nonce && strlen(nonce) > 32u) return DC_ERROR_INVALID_PARAM;

    const int has_query = (query != NULL);
    const int has_user_ids = (user_ids != NULL && user_id_count > 0u);
    if (has_query == has_user_ids) return DC_ERROR_INVALID_PARAM;
    if (has_user_ids && user_id_count > 100u) return DC_ERROR_INVALID_PARAM;

    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;
    yyjson_mut_val* root = doc.root;
    yyjson_mut_obj_add_int(doc.doc, root, "op", (int64_t)DC_GATEWAY_OP_REQUEST_GUILD_MEMBERS);

    yyjson_mut_val* d = yyjson_mut_obj(doc.doc);
    yyjson_mut_obj_add(root, yyjson_mut_strcpy(doc.doc, "d"), d);

    char guild_id_buf[32];
    st = dc_snowflake_to_cstr(guild_id, guild_id_buf, sizeof(guild_id_buf));
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }
    yyjson_mut_obj_add_strcpy(doc.doc, d, "guild_id", guild_id_buf);

    if (has_query) {
        yyjson_mut_obj_add_strcpy(doc.doc, d, "query", query);
        yyjson_mut_obj_add_int(doc.doc, d, "limit", (int64_t)limit);
    } else {
        yyjson_mut_val* ids = yyjson_mut_arr(doc.doc);
        for (size_t i = 0; i < user_id_count; i++) {
            if (!dc_snowflake_is_valid(user_ids[i])) {
                dc_json_mut_doc_free(&doc);
                return DC_ERROR_INVALID_PARAM;
            }
            char user_id_buf[32];
            st = dc_snowflake_to_cstr(user_ids[i], user_id_buf, sizeof(user_id_buf));
            if (st != DC_OK) {
                dc_json_mut_doc_free(&doc);
                return st;
            }
            yyjson_mut_arr_add_strcpy(doc.doc, ids, user_id_buf);
        }
        yyjson_mut_obj_add(d, yyjson_mut_strcpy(doc.doc, "user_ids"), ids);
    }

    if (presences) {
        yyjson_mut_val* presences_val = yyjson_mut_true(doc.doc);
        yyjson_mut_obj_add(d, yyjson_mut_strcpy(doc.doc, "presences"), presences_val);
    }
    if (nonce && nonce[0] != '\0') {
        yyjson_mut_obj_add_strcpy(doc.doc, d, "nonce", nonce);
    }

    st = dc_gateway_json_serialize(doc.doc, out);
    dc_json_mut_doc_free(&doc);
    return st;
}

static dc_status_t dc_gateway_build_request_soundboard_payload(const dc_snowflake_t* guild_ids,
                                                               size_t guild_id_count,
                                                               dc_string_t* out) {
    if (!guild_ids || guild_id_count == 0u || !out) return DC_ERROR_INVALID_PARAM;

    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;

    yyjson_mut_val* root = doc.root;
    yyjson_mut_obj_add_int(doc.doc, root, "op", (int64_t)DC_GATEWAY_OP_REQUEST_SOUNDBOARD_SOUNDS);

    yyjson_mut_val* d = yyjson_mut_obj(doc.doc);
    yyjson_mut_obj_add(root, yyjson_mut_strcpy(doc.doc, "d"), d);
    yyjson_mut_val* ids = yyjson_mut_arr(doc.doc);
    yyjson_mut_obj_add(d, yyjson_mut_strcpy(doc.doc, "guild_ids"), ids);

    for (size_t i = 0; i < guild_id_count; i++) {
        if (!dc_snowflake_is_valid(guild_ids[i])) {
            dc_json_mut_doc_free(&doc);
            return DC_ERROR_INVALID_PARAM;
        }
        char guild_id_buf[32];
        st = dc_snowflake_to_cstr(guild_ids[i], guild_id_buf, sizeof(guild_id_buf));
        if (st != DC_OK) {
            dc_json_mut_doc_free(&doc);
            return st;
        }
        yyjson_mut_arr_add_strcpy(doc.doc, ids, guild_id_buf);
    }

    st = dc_gateway_json_serialize(doc.doc, out);
    dc_json_mut_doc_free(&doc);
    return st;
}

static dc_status_t dc_gateway_build_voice_state_payload(dc_snowflake_t guild_id,
                                                        dc_snowflake_t channel_id,
                                                        int self_mute,
                                                        int self_deaf,
                                                        dc_string_t* out) {
    if (!dc_snowflake_is_valid(guild_id) || !out) return DC_ERROR_INVALID_PARAM;

    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;

    yyjson_mut_val* root = doc.root;
    yyjson_mut_obj_add_int(doc.doc, root, "op", (int64_t)DC_GATEWAY_OP_VOICE_STATE_UPDATE);

    yyjson_mut_val* d = yyjson_mut_obj(doc.doc);
    yyjson_mut_obj_add(root, yyjson_mut_strcpy(doc.doc, "d"), d);

    char guild_id_buf[32];
    st = dc_snowflake_to_cstr(guild_id, guild_id_buf, sizeof(guild_id_buf));
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }
    yyjson_mut_obj_add_strcpy(doc.doc, d, "guild_id", guild_id_buf);

    if (dc_snowflake_is_valid(channel_id)) {
        char channel_id_buf[32];
        st = dc_snowflake_to_cstr(channel_id, channel_id_buf, sizeof(channel_id_buf));
        if (st != DC_OK) {
            dc_json_mut_doc_free(&doc);
            return st;
        }
        yyjson_mut_obj_add_strcpy(doc.doc, d, "channel_id", channel_id_buf);
    } else {
        yyjson_mut_obj_add_null(doc.doc, d, "channel_id");
    }

    yyjson_mut_obj_add_bool(doc.doc, d, "self_mute", (bool)(self_mute != 0));
    yyjson_mut_obj_add_bool(doc.doc, d, "self_deaf", (bool)(self_deaf != 0));

    st = dc_gateway_json_serialize(doc.doc, out);
    dc_json_mut_doc_free(&doc);
    return st;
}

static dc_status_t dc_gateway_send_payload(dc_gateway_client_t* client,
                                           const dc_string_t* payload,
                                           int urgent,
                                           uint64_t due_ms,
                                           int opcode) {
    if (!client || !payload) return DC_ERROR_NULL_POINTER;
    return dc_gateway_outbox_push_at(client,
                                     dc_string_cstr(payload),
                                     dc_string_length(payload),
                                     urgent,
                                     due_ms,
                                     opcode);
}

static int dc_gateway_url_has_param(const char* url, const char* key) {
    const char* q = strchr(url, '?');
    if (!q) return 0;
    q++;
    size_t key_len = strlen(key);
    const char* p = q;
    while (*p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') return 1;
        const char* amp = strchr(p, '&');
        if (!amp) break;
        p = amp + 1;
    }
    return 0;
}

static dc_status_t dc_gateway_url_param_equals(const char* url, const char* key, const char* value) {
    const char* q = strchr(url, '?');
    if (!q) return DC_ERROR_NOT_FOUND;
    q++;
    size_t key_len = strlen(key);
    size_t val_len = strlen(value);
    const char* p = q;
    while (*p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            const char* v = p + key_len + 1;
            const char* end = strchr(v, '&');
            size_t len = end ? (size_t)(end - v) : strlen(v);
            if (len == val_len && strncmp(v, value, val_len) == 0) return DC_OK;
            return DC_ERROR_INVALID_PARAM;
        }
        const char* amp = strchr(p, '&');
        if (!amp) break;
        p = amp + 1;
    }
    return DC_ERROR_NOT_FOUND;
}

static dc_status_t dc_gateway_build_url(dc_gateway_client_t* client, const char* base, dc_string_t* out) {
    if (!client || !base || !out) return DC_ERROR_NULL_POINTER;
    if (strncmp(base, "wss://", (size_t)6) != 0) return DC_ERROR_INVALID_PARAM;

    dc_status_t st = dc_gateway_url_param_equals(base, "v", "10");
    if (st == DC_ERROR_INVALID_PARAM) return st;
    if (st == DC_ERROR_NOT_FOUND) {
        /* v will be appended */
    }
    st = dc_gateway_url_param_equals(base, "encoding", DC_GATEWAY_ENCODING);
    if (st == DC_ERROR_INVALID_PARAM) return st;

    if (dc_gateway_url_has_param(base, "compress")) {
        if (!client->enable_compression) return DC_ERROR_INVALID_PARAM;
        dc_status_t cst = dc_gateway_url_param_equals(base, "compress", "zlib-stream");
        if (cst == DC_ERROR_INVALID_PARAM) return cst;
    }

    dc_string_t tmp;
    st = dc_string_init_from_cstr(&tmp, base);
    if (st != DC_OK) return st;

    int has_q = strchr(base, '?') != NULL;
    if (!dc_gateway_url_has_param(base, "v")) {
        st = dc_string_append_cstr(&tmp, has_q ? "&" : "?");
        if (st != DC_OK) {
            dc_string_free(&tmp);
            return st;
        }
        st = dc_string_append_cstr(&tmp, "v=10");
        if (st != DC_OK) {
            dc_string_free(&tmp);
            return st;
        }
        has_q = 1;
    }
    if (!dc_gateway_url_has_param(base, "encoding")) {
        st = dc_string_append_cstr(&tmp, has_q ? "&" : "?");
        if (st != DC_OK) {
            dc_string_free(&tmp);
            return st;
        }
        st = dc_string_append_cstr(&tmp, "encoding=" DC_GATEWAY_ENCODING);
        if (st != DC_OK) {
            dc_string_free(&tmp);
            return st;
        }
        has_q = 1;
    }
    if (client->enable_compression && !dc_gateway_url_has_param(base, "compress")) {
        st = dc_string_append_cstr(&tmp, has_q ? "&" : "?");
        if (st != DC_OK) {
            dc_string_free(&tmp);
            return st;
        }
        st = dc_string_append_cstr(&tmp, DC_GATEWAY_COMPRESS_QUERY);
        if (st != DC_OK) {
            dc_string_free(&tmp);
            return st;
        }
    }

    dc_string_free(out);
    *out = tmp;
    return DC_OK;
}

static dc_status_t dc_gateway_store_ready_fields(dc_gateway_client_t* client, yyjson_val* d) {
    if (!client || !d) return DC_ERROR_NULL_POINTER;
    const char* resume_url = NULL;
    const char* session_id = NULL;
    if (dc_json_get_string(d, "resume_gateway_url", &resume_url) == DC_OK && resume_url) {
        dc_status_t st = dc_string_set_cstr(&client->resume_url, resume_url);
        if (st != DC_OK) return st;
    }
    if (dc_json_get_string(d, "session_id", &session_id) == DC_OK && session_id) {
        dc_status_t st = dc_string_set_cstr(&client->session_id, session_id);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

static dc_status_t dc_gateway_emit_event(dc_gateway_client_t* client, const char* name, yyjson_val* d) {
    if (!client || !name || !client->event_callback) return DC_OK;
    if (!d) return DC_OK;

    size_t json_len = 0;
    char* json = yyjson_val_write(d, 0, &json_len);
    if (!json) return DC_ERROR_JSON;
    dc_status_t st = dc_string_set_buffer(&client->event_buf, json, json_len);
    free(json);
    if (st != DC_OK) return st;
    client->event_callback(name, dc_string_cstr(&client->event_buf), client->user_data);
    return DC_OK;
}

static dc_status_t dc_gateway_handle_payload(dc_gateway_client_t* client, const char* data, size_t len) {
    if (!client || !data) return DC_ERROR_NULL_POINTER;

    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse_buffer(data, len, &doc);
    if (st != DC_OK) return st;

    yyjson_val* root = doc.root;
    int64_t op = 0;
    if (dc_json_get_int64(root, "op", &op) != DC_OK) {
        dc_json_doc_free(&doc);
        return DC_ERROR_INVALID_FORMAT;
    }

    dc_nullable_i64_t seq = {0};
    if (dc_json_get_int64_nullable(root, "s", &seq) == DC_OK && !seq.is_null) {
        if (!client->has_seq || seq.value > client->last_seq) {
            client->has_seq = 1;
            client->last_seq = seq.value;
        }
    }

    dc_nullable_cstr_t t = {0};
    if (dc_json_get_string_nullable(root, "t", &t) != DC_OK) {
        t.is_null = 1;
    }

    yyjson_val* d = yyjson_obj_get(root, "d");

    switch (op) {
        case DC_GATEWAY_OP_HELLO: {
            int64_t interval = 0;
            if (d && dc_json_get_int64(d, "heartbeat_interval", &interval) == DC_OK) {
                client->heartbeat_interval_ms = (uint32_t)interval;
                uint64_t now = dc_gateway_now_ms();
                double jitter = (double)rand() / (double)RAND_MAX;
                client->next_heartbeat_ms = now + (uint64_t)(jitter * (double)client->heartbeat_interval_ms);
                client->awaiting_heartbeat_ack = 0;
                client->last_heartbeat_ack_ms = now;
            }

            dc_string_t payload;
            dc_string_init(&payload);
            if (client->should_resume && client->has_seq &&
                dc_string_length(&client->session_id) > 0 &&
                dc_string_length(&client->resume_url) > 0) {
                st = dc_gateway_build_resume_payload(client, &payload);
                if (st == DC_OK) {
                    uint64_t now = dc_gateway_now_ms();
                    client->awaiting_heartbeat_ack = 0;
                    dc_gateway_send_payload(client, &payload, 1, now, DC_GATEWAY_OP_RESUME);
                    dc_gateway_set_state(client, DC_GATEWAY_RESUMING);
                    if (client->wsi) lws_callback_on_writable(client->wsi);
                }
            } else {
                st = dc_gateway_build_identify_payload(client, &payload);
                if (st == DC_OK) {
                    uint64_t now = dc_gateway_now_ms();
                    uint64_t due = now;
                    if (client->last_identify_ms > 0 &&
                        now - client->last_identify_ms < DC_GATEWAY_IDENTIFY_INTERVAL_MS) {
                        due = client->last_identify_ms + DC_GATEWAY_IDENTIFY_INTERVAL_MS;
                    }
                    client->identify_due_ms = due;
                    dc_gateway_send_payload(client, &payload, 1, due, DC_GATEWAY_OP_IDENTIFY);
                    dc_gateway_set_state(client, DC_GATEWAY_IDENTIFYING);
                    if (client->wsi) lws_callback_on_writable(client->wsi);
                }
            }
            dc_string_free(&payload);
            break;
        }
        case DC_GATEWAY_OP_HEARTBEAT: {
            dc_string_t payload;
            dc_string_init(&payload);
            st = dc_gateway_build_heartbeat_payload(client, &payload);
            if (st == DC_OK) {
                uint64_t now = dc_gateway_now_ms();
                dc_gateway_send_payload(client, &payload, 1, now, DC_GATEWAY_OP_HEARTBEAT);
            }
            dc_string_free(&payload);
            break;
        }
        case DC_GATEWAY_OP_HEARTBEAT_ACK:
            client->awaiting_heartbeat_ack = 0;
            client->last_heartbeat_ack_ms = dc_gateway_now_ms();
            break;
        case DC_GATEWAY_OP_RECONNECT:
            dc_gateway_schedule_reconnect(client);
            dc_gateway_set_state(client, DC_GATEWAY_RECONNECTING);
            break;
        case DC_GATEWAY_OP_INVALID_SESSION: {
            int resumable = 0;
            if (d && yyjson_is_bool(d)) {
                resumable = yyjson_get_bool(d) ? 1 : 0;
            }
            client->should_resume = resumable;
            if (!resumable) {
                dc_gateway_clear_session(client);
            }
            {
                uint64_t jitter = DC_GATEWAY_INVALID_SESSION_BACKOFF_MIN_MS;
                if (DC_GATEWAY_INVALID_SESSION_BACKOFF_MAX_MS >
                    DC_GATEWAY_INVALID_SESSION_BACKOFF_MIN_MS) {
                    uint64_t span = DC_GATEWAY_INVALID_SESSION_BACKOFF_MAX_MS -
                                    DC_GATEWAY_INVALID_SESSION_BACKOFF_MIN_MS;
                    jitter += (uint64_t)(rand() % (int)(span + 1));
                }
                client->reconnect_backoff_ms = (uint32_t)jitter;
                client->reconnect_at_ms = dc_gateway_now_ms() + jitter;
                client->reconnect_requested = 1;
                dc_gateway_set_state(client, DC_GATEWAY_RECONNECTING);
                dc_gateway_outbox_clear(client);
            }
            break;
        }
        case DC_GATEWAY_OP_DISPATCH:
            if (!t.is_null && t.value) {
                if (!seq.is_null) {
                    if (client->has_dispatch_seq && seq.value <= client->last_dispatch_seq) {
                        break;
                    }
                    client->has_dispatch_seq = 1;
                    client->last_dispatch_seq = seq.value;
                }
                if (strcmp(t.value, "READY") == 0 && d) {
                    dc_gateway_store_ready_fields(client, d);
                    client->should_resume = 1;
                    dc_gateway_set_state(client, DC_GATEWAY_READY);
                } else if (strcmp(t.value, "RESUMED") == 0) {
                    dc_gateway_set_state(client, DC_GATEWAY_READY);
                }
                dc_gateway_emit_event(client, t.value, d);
            }
            break;
        default:
            break;
    }

    dc_json_doc_free(&doc);
    return DC_OK;
}

static int dc_gateway_zlib_suffix_present(const dc_string_t* buf) {
    if (!buf || buf->length < DC_GATEWAY_ZLIB_SUFFIX_LEN) return 0;
    const unsigned char* p = (const unsigned char*)buf->data + (buf->length - DC_GATEWAY_ZLIB_SUFFIX_LEN);
    return p[0] == 0x00 && p[1] == 0x00 && p[2] == 0xff && p[3] == 0xff;
}

static dc_status_t dc_gateway_inflate(dc_gateway_client_t* client, dc_string_t* out) {
    if (!client || !out) return DC_ERROR_NULL_POINTER;
    if (!client->zinit) return DC_ERROR_INVALID_STATE;

    z_stream* zs = &client->zstrm;
    zs->next_in = (unsigned char*)client->compressed_buf.data;
    zs->avail_in = (uInt)client->compressed_buf.length;

    dc_string_clear(out);
    unsigned char tmp[4096];
    int ret = Z_OK;
    while (zs->avail_in > 0 && ret != Z_STREAM_END) {
        zs->next_out = tmp;
        zs->avail_out = (uInt)sizeof(tmp);
        ret = inflate(zs, Z_SYNC_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            return DC_ERROR_INVALID_FORMAT;
        }
        size_t produced = sizeof(tmp) - zs->avail_out;
        if (produced > 0) {
            dc_status_t st = dc_string_append_buffer(out, (const char*)tmp, produced);
            if (st != DC_OK) return st;
        }
    }
    return DC_OK;
}

static int dc_gateway_lws_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                   void* user, void* in, size_t len) {
    (void)user;
    dc_gateway_client_t* client = (dc_gateway_client_t*)lws_wsi_user(wsi);
    if (!client) return 0;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            dc_gateway_set_state(client, DC_GATEWAY_CONNECTED);
            client->reconnect_requested = 0;
            client->reconnect_backoff_ms = 0;
            client->connect_deadline_ms = 0;
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE: {
            const char* data = (const char*)in;
            if (client->enable_compression) {
                dc_status_t st = dc_string_append_buffer(&client->compressed_buf, data, len);
                if (st != DC_OK) {
                    client->last_error = st;
                    dc_string_clear(&client->compressed_buf);
                    break;
                }
                if (!dc_gateway_zlib_suffix_present(&client->compressed_buf)) {
                    break;
                }
                st = dc_gateway_inflate(client, &client->rx_buf);
                if (st == DC_OK) {
                    st = dc_gateway_handle_payload(client, client->rx_buf.data, client->rx_buf.length);
                }
                if (st != DC_OK) {
                    client->last_error = st;
                }
                dc_string_clear(&client->compressed_buf);
            } else {
                dc_status_t st = dc_string_append_buffer(&client->rx_buf, data, len);
                if (st != DC_OK) {
                    client->last_error = st;
                    dc_string_clear(&client->rx_buf);
                    break;
                }
                if (!lws_is_final_fragment(wsi)) {
                    break;
                }
                st = dc_gateway_handle_payload(client, client->rx_buf.data, client->rx_buf.length);
                if (st != DC_OK) {
                    client->last_error = st;
                }
                dc_string_clear(&client->rx_buf);
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            if (client->outbox.length == 0) break;
            uint64_t now = dc_gateway_now_ms();
            if (!dc_gateway_rate_limit_allows_send(client, now)) break;

            size_t idx = SIZE_MAX;
            for (size_t i = 0; i < client->outbox.length; i++) {
                dc_gateway_outgoing_t* candidate = (dc_gateway_outgoing_t*)dc_vec_at(&client->outbox, i);
                if (!candidate) continue;
                if (candidate->due_ms <= now) {
                    idx = i;
                    break;
                }
            }
            if (idx == SIZE_MAX) break;

            dc_gateway_outgoing_t msg;
            memset(&msg, 0, sizeof(msg));
            if (dc_vec_remove(&client->outbox, idx, &msg) != DC_OK) break;

            size_t payload_len = dc_string_length(&msg.payload);
            size_t buf_len = LWS_PRE + payload_len;
            size_t needed = buf_len < DC_GATEWAY_TX_INITIAL_CAP ? DC_GATEWAY_TX_INITIAL_CAP : buf_len;
            if (needed > client->tx_buf_cap) {
                unsigned char* next = (unsigned char*)dc_realloc(client->tx_buf, needed);
                if (!next) {
                    dc_gateway_outgoing_free(&msg);
                    client->last_error = DC_ERROR_OUT_OF_MEMORY;
                    break;
                }
                client->tx_buf = next;
                client->tx_buf_cap = needed;
            }
            if (client->tx_buf_cap == 0 || !client->tx_buf) {
                dc_gateway_outgoing_free(&msg);
                client->last_error = DC_ERROR_OUT_OF_MEMORY;
                break;
            }
            memcpy(client->tx_buf + LWS_PRE, msg.payload.data, payload_len);
            int wrote = lws_write(wsi, client->tx_buf + LWS_PRE, payload_len, LWS_WRITE_TEXT);
            dc_gateway_outgoing_free(&msg);
            if (wrote < 0) {
                client->last_error = DC_ERROR_WEBSOCKET;
                break;
            }
            if (msg.opcode == DC_GATEWAY_OP_IDENTIFY) {
                client->last_identify_ms = now;
            }
            dc_gateway_rate_limit_commit_send(client, now);
            if (client->outbox.length > 0) {
                lws_callback_on_writable(wsi);
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            client->last_error = DC_ERROR_WEBSOCKET;
            dc_gateway_set_state(client, DC_GATEWAY_DISCONNECTED);
            client->wsi = NULL;
            if (!client->manual_disconnect) {
                dc_gateway_schedule_reconnect(client);
            } else {
                client->manual_disconnect = 0;
            }
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            dc_gateway_handle_close(client, dc_gateway_get_close_code(wsi));
            dc_gateway_set_state(client, DC_GATEWAY_DISCONNECTED);
            client->wsi = NULL;
            if (!client->manual_disconnect) {
                dc_gateway_schedule_reconnect(client);
            } else {
                client->manual_disconnect = 0;
            }
            break;
        default:
            break;
    }
    return 0;
}

static struct lws_protocols dc_gateway_protocols[] = {
    {
        .name = "discord-gateway",
        .callback = dc_gateway_lws_callback,
        .per_session_data_size = 0,
        .rx_buffer_size = 0,
    },
    { .name = NULL, .callback = NULL, .per_session_data_size = 0, .rx_buffer_size = 0, .id = 0 }
};

static dc_status_t dc_gateway_context_ensure(dc_gateway_client_t* client) {
    if (client->context) return DC_OK;
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = dc_gateway_protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    client->context = lws_create_context(&info);
    if (!client->context) return DC_ERROR_WEBSOCKET;
    return DC_OK;
}

static dc_status_t dc_gateway_connect_url(dc_gateway_client_t* client, const char* url) {
    if (!client || !url) return DC_ERROR_NULL_POINTER;
    dc_status_t st = dc_gateway_build_url(client, url, &client->connect_url);
    if (st != DC_OK) return st;

    const char* proto = NULL;
    const char* address = NULL;
    const char* path = NULL;
    int port = 0;
    if (lws_parse_uri(client->connect_url.data, &proto, &address, &port, &path)) {
        return DC_ERROR_INVALID_PARAM;
    }

    struct lws_client_connect_info info;
    memset(&info, 0, sizeof(info));
    info.context = client->context;
    info.address = address;
    info.port = port;
    info.path = path;
    info.host = address;
    info.origin = address;
    info.ssl_connection = strcmp(proto, "wss") == 0 ? LCCSCF_USE_SSL : 0;
    info.protocol = dc_gateway_protocols[0].name;
    info.pwsi = &client->wsi;
    info.userdata = client;

    if (!lws_client_connect_via_info(&info)) {
        return DC_ERROR_WEBSOCKET;
    }

    dc_gateway_set_state(client, DC_GATEWAY_CONNECTING);
    if (client->connect_timeout_ms > 0) {
        client->connect_deadline_ms = dc_gateway_now_ms() + client->connect_timeout_ms;
    } else {
        client->connect_deadline_ms = 0;
    }
    return DC_OK;
}

dc_status_t dc_gateway_client_create(const dc_gateway_config_t* config, dc_gateway_client_t** client) {
    if (!config || !client) return DC_ERROR_NULL_POINTER;
    if (!config->token || config->token[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (config->shard_count == 0 && config->shard_id != 0) return DC_ERROR_INVALID_PARAM;
    if (config->shard_count > 0 && config->shard_id >= config->shard_count) return DC_ERROR_INVALID_PARAM;
    if (config->large_threshold > 0 &&
        (config->large_threshold < 50 || config->large_threshold > 250)) {
        return DC_ERROR_INVALID_PARAM;
    }
    if (config->enable_compression && config->enable_payload_compression) {
        return DC_ERROR_INVALID_PARAM;
    }
    if (config->user_agent && config->user_agent[0] != '\0') {
        if (!dc_http_user_agent_is_valid(config->user_agent)) {
            return DC_ERROR_INVALID_PARAM;
        }
    }

    *client = NULL;
    dc_gateway_client_t* c = (dc_gateway_client_t*)dc_alloc(sizeof(dc_gateway_client_t));
    if (!c) return DC_ERROR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));

    dc_status_t st = dc_string_init_from_cstr(&c->token, config->token);
    if (st != DC_OK) {
        dc_free(c);
        return st;
    }
    st = dc_string_init(&c->user_agent);
    if (st != DC_OK) {
        dc_string_free(&c->token);
        dc_free(c);
        return st;
    }
    if (config->user_agent && config->user_agent[0] != '\0') {
        st = dc_string_set_cstr(&c->user_agent, config->user_agent);
        if (st != DC_OK) {
            dc_string_free(&c->user_agent);
            dc_string_free(&c->token);
            dc_free(c);
            return st;
        }
    }

    c->intents = config->intents;
    c->shard_id = config->shard_id;
    c->shard_count = config->shard_count;
    c->large_threshold = config->large_threshold;
    c->event_callback = config->event_callback;
    c->state_callback = config->state_callback;
    c->user_data = config->user_data;
    c->heartbeat_timeout_ms = config->heartbeat_timeout_ms;
    c->connect_timeout_ms = config->connect_timeout_ms;
    c->enable_compression = config->enable_compression ? 1 : 0;
    c->enable_payload_compression = config->enable_payload_compression ? 1 : 0;
    c->state = DC_GATEWAY_DISCONNECTED;

    dc_string_init(&c->base_url);
    dc_string_init(&c->connect_url);
    dc_string_init(&c->resume_url);
    dc_string_init(&c->session_id);
    dc_string_init(&c->rx_buf);
    dc_string_init(&c->compressed_buf);
    dc_string_init(&c->event_buf);

    st = dc_string_reserve(&c->rx_buf, DC_GATEWAY_RX_INITIAL_CAP);
    if (st != DC_OK) {
        dc_gateway_client_free(c);
        return st;
    }
    st = dc_string_reserve(&c->compressed_buf, DC_GATEWAY_COMPRESSED_INITIAL_CAP);
    if (st != DC_OK) {
        dc_gateway_client_free(c);
        return st;
    }
    st = dc_string_reserve(&c->event_buf, DC_GATEWAY_EVENT_INITIAL_CAP);
    if (st != DC_OK) {
        dc_gateway_client_free(c);
        return st;
    }

    st = dc_vec_init(&c->outbox, sizeof(dc_gateway_outgoing_t));
    if (st != DC_OK) {
        dc_gateway_client_free(c);
        return st;
    }

    if (c->enable_compression) {
        memset(&c->zstrm, 0, sizeof(c->zstrm));
        int zret = inflateInit(&c->zstrm);
        if (zret != Z_OK) {
            dc_gateway_client_free(c);
            return DC_ERROR_INVALID_FORMAT;
        }
        c->zinit = 1;
    }

    *client = c;
    return DC_OK;
}

void dc_gateway_client_free(dc_gateway_client_t* client) {
    if (!client) return;
    if (client->wsi) {
        lws_set_timeout(client->wsi, PENDING_TIMEOUT_CLOSE_SEND, LWS_TO_KILL_ASYNC);
        client->wsi = NULL;
    }
    if (client->context) {
        lws_context_destroy(client->context);
        client->context = NULL;
    }
    if (client->zinit) {
        inflateEnd(&client->zstrm);
        client->zinit = 0;
    }
    dc_gateway_outbox_clear(client);
    dc_vec_free(&client->outbox);
    dc_free(client->tx_buf);
    client->tx_buf = NULL;
    client->tx_buf_cap = 0;
    dc_string_free(&client->event_buf);
    dc_string_free(&client->compressed_buf);
    dc_string_free(&client->rx_buf);
    dc_string_free(&client->session_id);
    dc_string_free(&client->resume_url);
    dc_string_free(&client->connect_url);
    dc_string_free(&client->base_url);
    dc_string_free(&client->user_agent);
    dc_string_free(&client->token);
    dc_free(client);
}

dc_status_t dc_gateway_client_connect(dc_gateway_client_t* client, const char* gateway_url) {
    if (!client) return DC_ERROR_NULL_POINTER;
    if (!gateway_url || gateway_url[0] == '\0') {
        if (dc_string_length(&client->resume_url) > 0 &&
            dc_string_length(&client->session_id) > 0) {
            gateway_url = dc_string_cstr(&client->resume_url);
            client->should_resume = 1;
        } else if (dc_string_length(&client->base_url) > 0) {
            gateway_url = dc_string_cstr(&client->base_url);
            client->should_resume = 0;
        } else {
            return DC_ERROR_INVALID_PARAM;
        }
    } else {
        dc_string_set_cstr(&client->base_url, gateway_url);
        client->should_resume = 0;
    }

    dc_status_t st = dc_gateway_context_ensure(client);
    if (st != DC_OK) return st;

    return dc_gateway_connect_url(client, gateway_url);
}

dc_status_t dc_gateway_client_disconnect(dc_gateway_client_t* client) {
    if (!client) return DC_ERROR_NULL_POINTER;
    if (client->wsi) {
        client->manual_disconnect = 1;
        lws_close_reason(client->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, (size_t)0);
        lws_callback_on_writable(client->wsi);
    }
    dc_gateway_set_state(client, DC_GATEWAY_DISCONNECTED);
    return DC_OK;
}

static void dc_gateway_maybe_send_heartbeat(dc_gateway_client_t* client) {
    if (!client || !client->wsi) return;
    if (client->heartbeat_interval_ms == 0) return;
    uint64_t now = dc_gateway_now_ms();
    if (now < client->next_heartbeat_ms) return;

    if (client->awaiting_heartbeat_ack) {
        uint64_t timeout_ms = client->heartbeat_timeout_ms > 0 ?
                              client->heartbeat_timeout_ms :
                              client->heartbeat_interval_ms;
        if (now - client->last_heartbeat_sent_ms > timeout_ms) {
            client->last_error = DC_ERROR_TIMEOUT;
            lws_close_reason(client->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, (size_t)0);
            lws_callback_on_writable(client->wsi);
            return;
        }
    }

    dc_string_t payload;
    dc_string_init(&payload);
    if (dc_gateway_build_heartbeat_payload(client, &payload) == DC_OK) {
        dc_gateway_send_payload(client, &payload, 1, now, DC_GATEWAY_OP_HEARTBEAT);
        client->last_heartbeat_sent_ms = now;
        client->awaiting_heartbeat_ack = 1;
        client->next_heartbeat_ms = now + client->heartbeat_interval_ms;
        lws_callback_on_writable(client->wsi);
    }
    dc_string_free(&payload);
}

dc_status_t dc_gateway_client_process(dc_gateway_client_t* client, uint32_t timeout_ms) {
    if (!client) return DC_ERROR_NULL_POINTER;
    if (!client->context) return DC_ERROR_INVALID_STATE;
    lws_service(client->context, (int)timeout_ms);

    if (client->state == DC_GATEWAY_CONNECTING && client->connect_deadline_ms > 0) {
        uint64_t now = dc_gateway_now_ms();
        if (now > client->connect_deadline_ms) {
            client->last_error = DC_ERROR_TIMEOUT;
            client->connect_deadline_ms = 0;
            if (client->wsi) {
                lws_close_reason(client->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, (size_t)0);
                lws_callback_on_writable(client->wsi);
            }
            dc_gateway_schedule_reconnect(client);
        }
    }

    dc_gateway_maybe_send_heartbeat(client);
    if (client->reconnect_requested && !client->wsi) {
        uint64_t now = dc_gateway_now_ms();
        if (now >= client->reconnect_at_ms) {
            dc_status_t st = dc_gateway_client_connect(client, NULL);
            if (st != DC_OK) {
                dc_gateway_schedule_reconnect(client);
            }
        }
    }
    if (client->wsi && client->outbox.length > 0) {
        uint64_t now = dc_gateway_now_ms();
        if (dc_gateway_rate_limit_allows_send(client, now) &&
            dc_gateway_outbox_has_ready(client, now)) {
            lws_callback_on_writable(client->wsi);
        }
    }

    if (client->last_error != DC_OK) {
        dc_status_t err = client->last_error;
        client->last_error = DC_OK;
        return err;
    }
    return DC_OK;
}

dc_status_t dc_gateway_client_get_state(const dc_gateway_client_t* client, dc_gateway_state_t* state) {
    if (!client || !state) return DC_ERROR_NULL_POINTER;
    *state = client->state;
    return DC_OK;
}

dc_status_t dc_gateway_client_update_presence(dc_gateway_client_t* client, const char* status,
                                              const char* activity_name, int activity_type) {
    if (!client || !status) return DC_ERROR_NULL_POINTER;
    if (!client->wsi) return DC_ERROR_INVALID_STATE;
    if (client->state != DC_GATEWAY_READY) return DC_ERROR_INVALID_STATE;
    dc_string_t payload;
    dc_string_init(&payload);
    dc_status_t st = dc_gateway_build_presence_payload(status, activity_name, activity_type, &payload);
    if (st == DC_OK) {
        uint64_t now = dc_gateway_now_ms();
        st = dc_gateway_send_payload(client, &payload, 1, now, DC_GATEWAY_OP_PRESENCE_UPDATE);
        if (st == DC_OK) {
            lws_callback_on_writable(client->wsi);
        }
    }
    dc_string_free(&payload);
    return st;
}

dc_status_t dc_gateway_client_request_guild_members(dc_gateway_client_t* client,
                                                    dc_snowflake_t guild_id,
                                                    const char* query,
                                                    uint32_t limit,
                                                    int presences,
                                                    const dc_snowflake_t* user_ids,
                                                    size_t user_id_count,
                                                    const char* nonce) {
    if (!client) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id)) return DC_ERROR_INVALID_PARAM;
    if (nonce && strlen(nonce) > 32u) return DC_ERROR_INVALID_PARAM;
    const int has_query = (query != NULL);
    const int has_user_ids = (user_ids != NULL && user_id_count > 0u);
    if (has_query == has_user_ids) return DC_ERROR_INVALID_PARAM;
    if (has_user_ids && user_id_count > 100u) return DC_ERROR_INVALID_PARAM;
    if (!client->wsi || client->state != DC_GATEWAY_READY) return DC_ERROR_INVALID_STATE;

    dc_string_t payload;
    dc_status_t st = dc_string_init(&payload);
    if (st != DC_OK) return st;

    st = dc_gateway_build_request_guild_members_payload(guild_id, query, limit, presences,
                                                        user_ids, user_id_count, nonce, &payload);
    if (st == DC_OK) {
        uint64_t now = dc_gateway_now_ms();
        st = dc_gateway_send_payload(client, &payload, 0, now, DC_GATEWAY_OP_REQUEST_GUILD_MEMBERS);
        if (st == DC_OK) {
            lws_callback_on_writable(client->wsi);
        }
    }
    dc_string_free(&payload);
    return st;
}

dc_status_t dc_gateway_client_request_soundboard_sounds(dc_gateway_client_t* client,
                                                        const dc_snowflake_t* guild_ids,
                                                        size_t guild_id_count) {
    if (!client) return DC_ERROR_NULL_POINTER;
    if (!guild_ids || guild_id_count == 0u) return DC_ERROR_INVALID_PARAM;
    if (!client->wsi || client->state != DC_GATEWAY_READY) return DC_ERROR_INVALID_STATE;

    dc_string_t payload;
    dc_status_t st = dc_string_init(&payload);
    if (st != DC_OK) return st;

    st = dc_gateway_build_request_soundboard_payload(guild_ids, guild_id_count, &payload);
    if (st == DC_OK) {
        uint64_t now = dc_gateway_now_ms();
        st = dc_gateway_send_payload(client, &payload, 0, now, DC_GATEWAY_OP_REQUEST_SOUNDBOARD_SOUNDS);
        if (st == DC_OK) {
            lws_callback_on_writable(client->wsi);
        }
    }
    dc_string_free(&payload);
    return st;
}

dc_status_t dc_gateway_client_update_voice_state(dc_gateway_client_t* client,
                                                 dc_snowflake_t guild_id,
                                                 dc_snowflake_t channel_id,
                                                 int self_mute,
                                                 int self_deaf) {
    if (!client) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id)) return DC_ERROR_INVALID_PARAM;
    if (!client->wsi || client->state != DC_GATEWAY_READY) return DC_ERROR_INVALID_STATE;

    dc_string_t payload;
    dc_status_t st = dc_string_init(&payload);
    if (st != DC_OK) return st;

    st = dc_gateway_build_voice_state_payload(guild_id, channel_id, self_mute, self_deaf, &payload);
    if (st == DC_OK) {
        uint64_t now = dc_gateway_now_ms();
        st = dc_gateway_send_payload(client, &payload, 1, now, DC_GATEWAY_OP_VOICE_STATE_UPDATE);
        if (st == DC_OK) {
            lws_callback_on_writable(client->wsi);
        }
    }
    dc_string_free(&payload);
    return st;
}
