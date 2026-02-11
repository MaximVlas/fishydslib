/**
 * @file dc_client.c
 * @brief Main client implementation
 */

#include "dc_client.h"
#include "core/dc_alloc.h"
#include "core/dc_status.h"
#include "http/dc_rest.h"
#include "json/dc_json.h"
#include "json/dc_json_model.h"
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <yyjson.h>

struct dc_client {
    dc_rest_client_t* rest;
    dc_gateway_client_t* gateway;
    int started;
    dc_http_auth_type_t auth_type;
    dc_log_callback_t log_callback;
    void* log_user_data;
    dc_log_level_t log_level;
};

static void dc_client_log(const dc_client_t* client, dc_log_level_t level, const char* fmt, ...) {
    if (!client || !client->log_callback) return;
    if (level > client->log_level) return;
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    client->log_callback(level, buf, client->log_user_data);
}

static dc_status_t dc_client_i64_to_u32(int64_t val, uint32_t* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (val < 0 || val > (int64_t)UINT32_MAX) return DC_ERROR_INVALID_FORMAT;
    *out = (uint32_t)val;
    return DC_OK;
}

static dc_status_t dc_client_double_ms_to_u32(double val, uint32_t* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (val < 0.0 || val > (double)UINT32_MAX) return DC_ERROR_INVALID_FORMAT;
    uint32_t trunc = (uint32_t)val;
    if ((double)trunc < val && trunc < UINT32_MAX) {
        trunc++;
    }
    *out = trunc;
    return DC_OK;
}

static dc_status_t dc_client_double_to_u32_exact(double val, uint32_t* out) {
    if (!out) return DC_ERROR_NULL_POINTER;
    if (val < 0.0 || val > (double)UINT32_MAX) return DC_ERROR_INVALID_FORMAT;
    uint32_t trunc = (uint32_t)val;
    if ((double)trunc != val) return DC_ERROR_INVALID_FORMAT;
    *out = trunc;
    return DC_OK;
}

static dc_status_t dc_client_parse_user(const char* body, size_t len, dc_user_t* user) {
    if (!body || !user) return DC_ERROR_NULL_POINTER;
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse_buffer(body, len, &doc);
    if (st != DC_OK) return st;

    dc_user_t tmp;
    st = dc_user_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    st = dc_json_model_user_from_val(doc.root, &tmp);
    dc_json_doc_free(&doc);
    if (st != DC_OK) {
        dc_user_free(&tmp);
        return st;
    }

    *user = tmp;
    return DC_OK;
}

static dc_status_t dc_client_parse_message_id(const char* body, size_t len, dc_snowflake_t* message_id) {
    if (!body || !message_id) return DC_ERROR_NULL_POINTER;
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse_buffer(body, len, &doc);
    if (st != DC_OK) return st;

    uint64_t id = 0;
    st = dc_json_get_snowflake(doc.root, "id", &id);
    dc_json_doc_free(&doc);
    if (st != DC_OK) return st;
    *message_id = id;
    return DC_OK;
}

static dc_status_t dc_client_parse_channel(const char* body, size_t len, dc_channel_t* channel) {
    if (!body || !channel) return DC_ERROR_NULL_POINTER;
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse_buffer(body, len, &doc);
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

static dc_status_t dc_client_parse_channel_list(const char* body,
                                                size_t len,
                                                dc_channel_list_t* channels) {
    if (!body || !channels) return DC_ERROR_NULL_POINTER;

    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse_buffer(body, len, &doc);
    if (st != DC_OK) return st;
    if (!yyjson_is_arr(doc.root)) {
        dc_json_doc_free(&doc);
        return DC_ERROR_INVALID_FORMAT;
    }

    dc_channel_list_t tmp;
    st = dc_channel_list_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    yyjson_arr_iter iter = yyjson_arr_iter_with(doc.root);
    yyjson_val* item = NULL;
    while ((item = yyjson_arr_iter_next(&iter))) {
        if (!yyjson_is_obj(item)) {
            st = DC_ERROR_INVALID_FORMAT;
            goto cleanup_channel_list;
        }

        dc_channel_t channel;
        st = dc_channel_init(&channel);
        if (st != DC_OK) goto cleanup_channel_list;

        st = dc_json_model_channel_from_val(item, &channel);
        if (st != DC_OK) {
            dc_channel_free(&channel);
            goto cleanup_channel_list;
        }

        st = dc_vec_push(&tmp.items, &channel);
        if (st != DC_OK) {
            dc_channel_free(&channel);
            goto cleanup_channel_list;
        }

        memset(&channel, 0, sizeof(channel));
    }

    dc_json_doc_free(&doc);
    *channels = tmp;
    return DC_OK;

cleanup_channel_list:
    dc_json_doc_free(&doc);
    dc_channel_list_free(&tmp);
    return st;
}

static dc_status_t dc_client_parse_guild_member(const char* body,
                                                size_t len,
                                                dc_guild_member_t* member) {
    if (!body || !member) return DC_ERROR_NULL_POINTER;

    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse_buffer(body, len, &doc);
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

static dc_status_t dc_client_parse_guild_member_list(const char* body,
                                                     size_t len,
                                                     dc_guild_member_list_t* members) {
    if (!body || !members) return DC_ERROR_NULL_POINTER;

    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse_buffer(body, len, &doc);
    if (st != DC_OK) return st;
    if (!yyjson_is_arr(doc.root)) {
        dc_json_doc_free(&doc);
        return DC_ERROR_INVALID_FORMAT;
    }

    dc_guild_member_list_t tmp;
    st = dc_guild_member_list_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    yyjson_arr_iter iter = yyjson_arr_iter_with(doc.root);
    yyjson_val* item = NULL;
    while ((item = yyjson_arr_iter_next(&iter))) {
        if (!yyjson_is_obj(item)) {
            st = DC_ERROR_INVALID_FORMAT;
            goto cleanup_member_list;
        }

        dc_guild_member_t member;
        st = dc_guild_member_init(&member);
        if (st != DC_OK) goto cleanup_member_list;

        st = dc_json_model_guild_member_from_val(item, &member);
        if (st != DC_OK) {
            dc_guild_member_free(&member);
            goto cleanup_member_list;
        }

        st = dc_vec_push(&tmp.items, &member);
        if (st != DC_OK) {
            dc_guild_member_free(&member);
            goto cleanup_member_list;
        }

        memset(&member, 0, sizeof(member));
    }

    dc_json_doc_free(&doc);
    *members = tmp;
    return DC_OK;

cleanup_member_list:
    dc_json_doc_free(&doc);
    dc_guild_member_list_free(&tmp);
    return st;
}

static dc_status_t dc_client_parse_role(const char* body, size_t len, dc_role_t* role) {
    if (!body || !role) return DC_ERROR_NULL_POINTER;

    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse_buffer(body, len, &doc);
    if (st != DC_OK) return st;

    dc_role_t tmp;
    st = dc_role_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    st = dc_json_model_role_from_val(doc.root, &tmp);
    dc_json_doc_free(&doc);
    if (st != DC_OK) {
        dc_role_free(&tmp);
        return st;
    }

    *role = tmp;
    return DC_OK;
}

static dc_status_t dc_client_parse_role_list(const char* body,
                                             size_t len,
                                             dc_role_list_t* roles) {
    if (!body || !roles) return DC_ERROR_NULL_POINTER;

    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse_buffer(body, len, &doc);
    if (st != DC_OK) return st;
    if (!yyjson_is_arr(doc.root)) {
        dc_json_doc_free(&doc);
        return DC_ERROR_INVALID_FORMAT;
    }

    dc_role_list_t tmp;
    st = dc_role_list_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    yyjson_arr_iter iter = yyjson_arr_iter_with(doc.root);
    yyjson_val* item = NULL;
    while ((item = yyjson_arr_iter_next(&iter))) {
        if (!yyjson_is_obj(item)) {
            st = DC_ERROR_INVALID_FORMAT;
            goto cleanup_role_list;
        }

        dc_role_t role;
        st = dc_role_init(&role);
        if (st != DC_OK) goto cleanup_role_list;

        st = dc_json_model_role_from_val(item, &role);
        if (st != DC_OK) {
            dc_role_free(&role);
            goto cleanup_role_list;
        }

        st = dc_vec_push(&tmp.items, &role);
        if (st != DC_OK) {
            dc_role_free(&role);
            goto cleanup_role_list;
        }

        memset(&role, 0, sizeof(role));
    }

    dc_json_doc_free(&doc);
    *roles = tmp;
    return DC_OK;

cleanup_role_list:
    dc_json_doc_free(&doc);
    dc_role_list_free(&tmp);
    return st;
}

static dc_status_t dc_client_parse_message(const char* body, size_t len, dc_message_t* message) {
    if (!body || !message) return DC_ERROR_NULL_POINTER;
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse_buffer(body, len, &doc);
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

static dc_status_t dc_client_parse_guild(const char* body, size_t len, dc_guild_t* guild) {
    if (!body || !guild) return DC_ERROR_NULL_POINTER;
    (void)len;
    return dc_guild_from_json(body, guild);
}

static dc_status_t dc_client_execute_json_request(dc_client_t* client,
                                                  dc_http_method_t method,
                                                  const char* path,
                                                  const char* json_body,
                                                  int is_interaction,
                                                  dc_rest_response_t* out_resp) {
    if (!client || !client->rest || !path || !out_resp) return DC_ERROR_NULL_POINTER;

    dc_rest_request_t req;
    dc_status_t st = dc_rest_request_init(&req);
    if (st != DC_OK) return st;

    st = dc_rest_request_set_method(&req, method);
    if (st != DC_OK) goto cleanup;
    st = dc_rest_request_set_path(&req, path);
    if (st != DC_OK) goto cleanup;
    if (json_body) {
        st = dc_rest_request_set_json_body(&req, json_body);
        if (st != DC_OK) goto cleanup;
    }
    if (is_interaction) {
        st = dc_rest_request_set_interaction(&req, 1);
        if (st != DC_OK) goto cleanup;
    }

    st = dc_rest_execute(client->rest, &req, out_resp);
    if (st != DC_OK) goto cleanup;
    if (out_resp->http.status_code < 200 || out_resp->http.status_code >= 300) {
        st = dc_status_from_http(out_resp->http.status_code);
    }

cleanup:
    dc_rest_request_free(&req);
    return st;
}

static dc_status_t dc_client_response_body_to_json_out(const dc_rest_response_t* resp, dc_string_t* out_json) {
    if (!resp || !out_json) return DC_ERROR_NULL_POINTER;
    return dc_string_set_cstr(out_json, dc_string_cstr(&resp->http.body));
}

static dc_status_t dc_client_json_content_body(const char* content, int ephemeral, dc_string_t* out_json) {
    if (!content || !out_json) return DC_ERROR_NULL_POINTER;

    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;
    yyjson_mut_val* root = doc.root;
    yyjson_mut_obj_add_strcpy(doc.doc, root, "content", content);
    if (ephemeral) {
        yyjson_mut_obj_add_int(doc.doc, root, "flags", (int64_t)64);
    }
    st = dc_json_mut_doc_serialize(&doc, out_json);
    dc_json_mut_doc_free(&doc);
    return st;
}

static dc_status_t dc_client_interaction_webhook_request(dc_client_t* client,
                                                         dc_http_method_t method,
                                                         const char* path,
                                                         const char* json_body,
                                                         dc_rest_response_t* out_resp) {
    return dc_client_execute_json_request(client, method, path, json_body, 1, out_resp);
}

static dc_status_t dc_client_execute_json_request_out(dc_client_t* client,
                                                      dc_http_method_t method,
                                                      const char* path,
                                                      const char* json_body,
                                                      int is_interaction,
                                                      dc_string_t* out_json) {
    dc_rest_response_t resp;
    dc_status_t st = dc_rest_response_init(&resp);
    if (st != DC_OK) return st;

    st = dc_client_execute_json_request(client, method, path, json_body, is_interaction, &resp);
    if (st == DC_OK && out_json) {
        st = dc_string_set_cstr(out_json, dc_string_cstr(&resp.http.body));
    }

    dc_rest_response_free(&resp);
    return st;
}

static dc_status_t dc_client_snowflake_to_buf(dc_snowflake_t id, char out[32]) {
    if (!dc_snowflake_is_valid(id)) return DC_ERROR_INVALID_PARAM;
    return dc_snowflake_to_cstr(id, out, 32);
}

dc_status_t dc_gateway_info_init(dc_gateway_info_t* info) {
    if (!info) return DC_ERROR_NULL_POINTER;
    memset(info, 0, sizeof(*info));
    return dc_string_init(&info->url);
}

void dc_gateway_info_free(dc_gateway_info_t* info) {
    if (!info) return;
    dc_string_free(&info->url);
    memset(info, 0, sizeof(*info));
}

void dc_client_config_init(dc_client_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->auth_type = DC_HTTP_AUTH_BOT;
    config->http_timeout_ms = 30000;
    config->gateway_timeout_ms = 60000;
    config->log_level = DC_LOG_INFO;
}

dc_status_t dc_client_config_set_user_agent_info(dc_client_config_t* config, const dc_user_agent_t* ua) {
    if (!config || !ua) return DC_ERROR_NULL_POINTER;
    config->user_agent_info = *ua;
    config->use_user_agent_info = 1;
    config->user_agent = NULL;
    return DC_OK;
}

dc_status_t dc_client_create(const dc_client_config_t* config, dc_client_t** client) {
    if (!config || !client) return DC_ERROR_NULL_POINTER;
    if (!config->token || config->token[0] == '\0') return DC_ERROR_INVALID_PARAM;
    *client = NULL;

    dc_client_t* c = (dc_client_t*)dc_alloc(sizeof(dc_client_t));
    if (!c) return DC_ERROR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));

    c->log_callback = config->log_callback;
    c->log_user_data = config->log_user_data;
    c->log_level = config->log_level;

    const char* user_agent = config->user_agent;
    dc_string_t ua_buf;
    int ua_inited = 0;
    dc_status_t st = DC_OK;
    if ((!user_agent || user_agent[0] == '\0') && config->use_user_agent_info) {
        st = dc_string_init(&ua_buf);
        if (st != DC_OK) {
            dc_free(c);
            return st;
        }
        ua_inited = 1;
        st = dc_http_format_user_agent(&config->user_agent_info, &ua_buf);
        if (st != DC_OK) {
            dc_string_free(&ua_buf);
            dc_free(c);
            return st;
        }
        user_agent = dc_string_cstr(&ua_buf);
    }

    dc_rest_client_config_t rest_cfg;
    memset(&rest_cfg, 0, sizeof(rest_cfg));
    rest_cfg.token = config->token;
    rest_cfg.auth_type = config->auth_type;
    rest_cfg.user_agent = user_agent;
    rest_cfg.timeout_ms = config->http_timeout_ms;

    st = dc_rest_client_create(&rest_cfg, &c->rest);
    if (st != DC_OK) {
        if (ua_inited) dc_string_free(&ua_buf);
        dc_free(c);
        return st;
    }

    dc_gateway_config_t gw_cfg;
    memset(&gw_cfg, 0, sizeof(gw_cfg));
    gw_cfg.token = config->token;
    gw_cfg.intents = config->intents;
    gw_cfg.shard_id = config->shard_id;
    gw_cfg.shard_count = config->shard_count;
    gw_cfg.large_threshold = config->large_threshold;
    gw_cfg.user_agent = user_agent;
    gw_cfg.event_callback = config->event_callback;
    gw_cfg.state_callback = config->state_callback;
    gw_cfg.user_data = config->user_data;
    gw_cfg.heartbeat_timeout_ms = config->gateway_timeout_ms;
    gw_cfg.connect_timeout_ms = config->gateway_timeout_ms;
    gw_cfg.enable_compression = config->enable_compression;
    gw_cfg.enable_payload_compression = config->enable_payload_compression;

    st = dc_gateway_client_create(&gw_cfg, &c->gateway);
    if (st != DC_OK) {
        dc_rest_client_free(c->rest);
        if (ua_inited) dc_string_free(&ua_buf);
        dc_free(c);
        return st;
    }

    c->started = 0;
    c->auth_type = config->auth_type;
    if (ua_inited) dc_string_free(&ua_buf);
    dc_client_log(c, DC_LOG_DEBUG,
                  "Config intents=0x%08x shard=%u/%u large_threshold=%u compression=%d payload_compression=%d",
                  config->intents,
                  config->shard_id,
                  config->shard_count,
                  config->large_threshold,
                  config->enable_compression,
                  config->enable_payload_compression);
    dc_client_log(c, DC_LOG_INFO, "Client created");
    *client = c;
    return DC_OK;
}

dc_status_t dc_client_init(const dc_client_config_t* config, dc_client_t** client) {
    return dc_client_create(config, client);
}

void dc_client_free(dc_client_t* client) {
    if (!client) return;
    if (client->gateway) {
        dc_gateway_client_free(client->gateway);
        client->gateway = NULL;
    }
    if (client->rest) {
        dc_rest_client_free(client->rest);
        client->rest = NULL;
    }
    dc_free(client);
}

void dc_client_shutdown(dc_client_t* client) {
    dc_client_free(client);
}

void dc_client_set_logger(dc_client_t* client, dc_log_callback_t callback, void* user_data, dc_log_level_t level) {
    if (!client) return;
    client->log_callback = callback;
    client->log_user_data = user_data;
    client->log_level = level;
}

dc_status_t dc_client_start(dc_client_t* client) {
    if (!client || !client->gateway || !client->rest) return DC_ERROR_NULL_POINTER;
    if (client->started) return DC_ERROR_INVALID_STATE;

    dc_client_log(client, DC_LOG_INFO, "Starting client");
    dc_client_log(client, DC_LOG_DEBUG, "Fetching gateway info via REST");

    dc_gateway_info_t info;
    dc_status_t st = dc_gateway_info_init(&info);
    if (st != DC_OK) return st;

    st = dc_client_get_gateway_info(client, &info);
    if (st != DC_OK) {
        dc_client_log(client, DC_LOG_ERROR, "Failed to get gateway info: %s", dc_status_string(st));
        dc_gateway_info_free(&info);
        return st;
    }

    dc_client_log(client, DC_LOG_DEBUG, "Gateway URL: %s", dc_string_cstr(&info.url));
    st = dc_gateway_client_connect(client->gateway, dc_string_cstr(&info.url));
    dc_gateway_info_free(&info);
    if (st != DC_OK) return st;

    client->started = 1;
    return DC_OK;
}

dc_status_t dc_client_start_with_gateway_url(dc_client_t* client, const char* gateway_url) {
    if (!client || !client->gateway) return DC_ERROR_NULL_POINTER;
    if (!gateway_url || gateway_url[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (client->started) return DC_ERROR_INVALID_STATE;

    dc_client_log(client, DC_LOG_INFO, "Starting client with gateway URL");
    dc_client_log(client, DC_LOG_DEBUG, "Gateway URL: %s", gateway_url);
    dc_status_t st = dc_gateway_client_connect(client->gateway, gateway_url);
    if (st != DC_OK) return st;
    client->started = 1;
    return DC_OK;
}

dc_status_t dc_client_stop(dc_client_t* client) {
    if (!client || !client->gateway) return DC_ERROR_NULL_POINTER;
    dc_client_log(client, DC_LOG_INFO, "Stopping client");
    dc_status_t st = dc_gateway_client_disconnect(client->gateway);
    if (st == DC_OK) {
        client->started = 0;
    }
    return st;
}

dc_status_t dc_client_process(dc_client_t* client, uint32_t timeout_ms) {
    if (!client || !client->gateway) return DC_ERROR_NULL_POINTER;
    dc_client_log(client, DC_LOG_TRACE, "Process tick timeout_ms=%u", timeout_ms);
    dc_status_t st = dc_gateway_client_process(client->gateway, timeout_ms);
    if (st != DC_OK && st != DC_ERROR_TIMEOUT) {
        dc_client_log(client, DC_LOG_WARN, "Gateway process error: %s", dc_status_string(st));
    }
    return st;
}

dc_status_t dc_client_get_gateway_info(dc_client_t* client, dc_gateway_info_t* info) {
    if (!client || !client->rest || !info) return DC_ERROR_NULL_POINTER;

    dc_rest_request_t req;
    dc_rest_response_t resp;
    dc_status_t st = dc_rest_request_init(&req);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_rest_request_free(&req);
        return st;
    }

    st = dc_rest_request_set_method(&req, DC_HTTP_GET);
    if (st != DC_OK) goto cleanup;
    const char* gw_path = (client->auth_type == DC_HTTP_AUTH_BOT) ? "/gateway/bot" : "/gateway";
    st = dc_rest_request_set_path(&req, gw_path);
    if (st != DC_OK) goto cleanup;

    st = dc_rest_execute(client->rest, &req, &resp);
    if (st != DC_OK) goto cleanup;

    if (resp.http.status_code < 200 || resp.http.status_code >= 300) {
        dc_client_log(client, DC_LOG_WARN, "Gateway info HTTP %d", resp.http.status_code);
        st = dc_status_from_http(resp.http.status_code);
        goto cleanup;
    }

    dc_json_doc_t doc;
    st = dc_json_parse_buffer(dc_string_cstr(&resp.http.body), dc_string_length(&resp.http.body), &doc);
    if (st != DC_OK) goto cleanup;

    dc_gateway_info_t tmp;
    st = dc_gateway_info_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        goto cleanup;
    }

    const char* url = NULL;
    st = dc_json_get_string(doc.root, "url", &url);
    if (st != DC_OK) {
        dc_client_log(client, DC_LOG_ERROR, "Gateway info parse failed: url (%s)", dc_status_string(st));
        dc_gateway_info_free(&tmp);
        dc_json_doc_free(&doc);
        goto cleanup;
    }
    st = dc_string_set_cstr(&tmp.url, url);
    if (st != DC_OK) {
        dc_gateway_info_free(&tmp);
        dc_json_doc_free(&doc);
        goto cleanup;
    }

    double shards_num = 0.0;
    st = dc_json_get_double_opt(doc.root, "shards", &shards_num, 0.0);
    if (st != DC_OK) {
        dc_client_log(client, DC_LOG_ERROR, "Gateway info parse failed: shards (%s)", dc_status_string(st));
        dc_gateway_info_free(&tmp);
        dc_json_doc_free(&doc);
        goto cleanup;
    }
    st = dc_client_double_to_u32_exact(shards_num, &tmp.shards);
    if (st != DC_OK) {
        dc_client_log(client, DC_LOG_ERROR, "Gateway info parse failed: shards range/value");
        dc_gateway_info_free(&tmp);
        dc_json_doc_free(&doc);
        goto cleanup;
    }

    yyjson_val* limit = NULL;
    st = dc_json_get_object_opt(doc.root, "session_start_limit", &limit);
    if (st != DC_OK) {
        dc_client_log(client, DC_LOG_ERROR, "Gateway info parse failed: session_start_limit (%s)", dc_status_string(st));
        dc_gateway_info_free(&tmp);
        dc_json_doc_free(&doc);
        goto cleanup;
    }
    if (limit) {
        double total_num = 0.0;
        double remaining_num = 0.0;
        double reset_ms = 0.0;
        double max_conc_num = 0.0;

        st = dc_json_get_double_opt(limit, "total", &total_num, 0.0);
        if (st != DC_OK) {
            dc_client_log(client, DC_LOG_ERROR, "Gateway info parse failed: session_start_limit.total (%s)", dc_status_string(st));
            dc_gateway_info_free(&tmp);
            dc_json_doc_free(&doc);
            goto cleanup;
        }
        st = dc_json_get_double_opt(limit, "remaining", &remaining_num, 0.0);
        if (st != DC_OK) {
            dc_client_log(client, DC_LOG_ERROR, "Gateway info parse failed: session_start_limit.remaining (%s)", dc_status_string(st));
            dc_gateway_info_free(&tmp);
            dc_json_doc_free(&doc);
            goto cleanup;
        }
        st = dc_json_get_double_opt(limit, "reset_after", &reset_ms, 0.0);
        if (st != DC_OK) {
            dc_client_log(client, DC_LOG_ERROR, "Gateway info parse failed: session_start_limit.reset_after (%s)", dc_status_string(st));
            dc_gateway_info_free(&tmp);
            dc_json_doc_free(&doc);
            goto cleanup;
        }
        st = dc_json_get_double_opt(limit, "max_concurrency", &max_conc_num, 0.0);
        if (st != DC_OK) {
            dc_client_log(client, DC_LOG_ERROR, "Gateway info parse failed: session_start_limit.max_concurrency (%s)", dc_status_string(st));
            dc_gateway_info_free(&tmp);
            dc_json_doc_free(&doc);
            goto cleanup;
        }

        st = dc_client_double_to_u32_exact(total_num, &tmp.session_limit_total);
        if (st != DC_OK) {
            dc_client_log(client, DC_LOG_ERROR, "Gateway info parse failed: session_start_limit.total range/value");
            dc_gateway_info_free(&tmp);
            dc_json_doc_free(&doc);
            goto cleanup;
        }
        st = dc_client_double_to_u32_exact(remaining_num, &tmp.session_limit_remaining);
        if (st != DC_OK) {
            dc_client_log(client, DC_LOG_ERROR, "Gateway info parse failed: session_start_limit.remaining range/value");
            dc_gateway_info_free(&tmp);
            dc_json_doc_free(&doc);
            goto cleanup;
        }
        st = dc_client_double_ms_to_u32(reset_ms, &tmp.session_limit_reset_after_ms);
        if (st != DC_OK) {
            dc_client_log(client, DC_LOG_ERROR, "Gateway info parse failed: session_start_limit.reset_after range/value");
            dc_gateway_info_free(&tmp);
            dc_json_doc_free(&doc);
            goto cleanup;
        }
        st = dc_client_double_to_u32_exact(max_conc_num, &tmp.session_limit_max_concurrency);
        if (st != DC_OK) {
            dc_client_log(client, DC_LOG_ERROR, "Gateway info parse failed: session_start_limit.max_concurrency range/value");
            dc_gateway_info_free(&tmp);
            dc_json_doc_free(&doc);
            goto cleanup;
        }
    }

    dc_client_log(client, DC_LOG_DEBUG,
                  "Gateway info shards=%u session_limit=%u remaining=%u reset_after_ms=%u max_concurrency=%u",
                  tmp.shards,
                  tmp.session_limit_total,
                  tmp.session_limit_remaining,
                  tmp.session_limit_reset_after_ms,
                  tmp.session_limit_max_concurrency);

    dc_json_doc_free(&doc);
    *info = tmp;

cleanup:
    dc_rest_response_free(&resp);
    dc_rest_request_free(&req);
    return st;
}

dc_status_t dc_client_get_current_user(dc_client_t* client, dc_user_t* user) {
    if (!client || !client->rest || !user) return DC_ERROR_NULL_POINTER;

    dc_rest_request_t req;
    dc_rest_response_t resp;
    dc_status_t st = dc_rest_request_init(&req);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_rest_request_free(&req);
        return st;
    }

    st = dc_rest_request_set_method(&req, DC_HTTP_GET);
    if (st != DC_OK) goto cleanup;
    st = dc_rest_request_set_path(&req, "/users/@me");
    if (st != DC_OK) goto cleanup;

    st = dc_rest_execute(client->rest, &req, &resp);
    if (st != DC_OK) goto cleanup;

    if (resp.http.status_code < 200 || resp.http.status_code >= 300) {
        dc_client_log(client, DC_LOG_WARN, "Create message HTTP %d", resp.http.status_code);
        st = dc_status_from_http(resp.http.status_code);
        goto cleanup;
    }

    st = dc_client_parse_user(dc_string_cstr(&resp.http.body),
                              dc_string_length(&resp.http.body), user);

cleanup:
    dc_rest_response_free(&resp);
    dc_rest_request_free(&req);
    return st;
}

dc_status_t dc_client_get_user(dc_client_t* client, dc_snowflake_t user_id, dc_user_t* user) {
    if (!client || !client->rest || !user) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(user_id)) return DC_ERROR_INVALID_PARAM;

    char id_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(user_id, id_buf, sizeof(id_buf));
    if (st != DC_OK) return st;

    dc_rest_request_t req;
    dc_rest_response_t resp;
    st = dc_rest_request_init(&req);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_rest_request_free(&req);
        return st;
    }

    st = dc_rest_request_set_method(&req, DC_HTTP_GET);
    if (st != DC_OK) goto cleanup;
    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) goto cleanup;
    st = dc_string_printf(&path, "/users/%s", id_buf);
    if (st != DC_OK) {
        dc_string_free(&path);
        goto cleanup;
    }
    st = dc_rest_request_set_path(&req, dc_string_cstr(&path));
    dc_string_free(&path);
    if (st != DC_OK) goto cleanup;

    st = dc_rest_execute(client->rest, &req, &resp);
    if (st != DC_OK) goto cleanup;

    if (resp.http.status_code < 200 || resp.http.status_code >= 300) {
        st = dc_status_from_http(resp.http.status_code);
        goto cleanup;
    }

    st = dc_client_parse_user(dc_string_cstr(&resp.http.body),
                              dc_string_length(&resp.http.body), user);

cleanup:
    dc_rest_response_free(&resp);
    dc_rest_request_free(&req);
    return st;
}

dc_status_t dc_client_create_message(dc_client_t* client, dc_snowflake_t channel_id,
                                     const char* content, dc_snowflake_t* message_id) {
    if (!content) return DC_ERROR_NULL_POINTER;
    if (content[0] == '\0') return DC_ERROR_INVALID_PARAM;

    dc_client_log(client, DC_LOG_DEBUG, "Create message channel=%llu len=%zu",
                  (unsigned long long)channel_id, strlen(content));

    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_string(&doc, doc.root, "content", content);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }

    dc_string_t json_body;
    st = dc_string_init(&json_body);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }
    st = dc_json_mut_doc_serialize(&doc, &json_body);
    dc_json_mut_doc_free(&doc);
    if (st != DC_OK) {
        dc_string_free(&json_body);
        return st;
    }

    st = dc_client_create_message_json(client, channel_id, dc_string_cstr(&json_body), message_id);
    dc_string_free(&json_body);
    return st;
}

dc_status_t dc_client_create_message_json(dc_client_t* client,
                                          dc_snowflake_t channel_id,
                                          const char* json_body,
                                          dc_snowflake_t* message_id) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id)) return DC_ERROR_INVALID_PARAM;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char id_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, id_buf, sizeof(id_buf));
    if (st != DC_OK) return st;

    dc_rest_request_t req;
    dc_rest_response_t resp;
    st = dc_rest_request_init(&req);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_rest_request_free(&req);
        return st;
    }

    st = dc_rest_request_set_method(&req, DC_HTTP_POST);
    if (st != DC_OK) goto cleanup;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) goto cleanup;
    st = dc_string_printf(&path, "/channels/%s/messages", id_buf);
    if (st != DC_OK) {
        dc_string_free(&path);
        goto cleanup;
    }
    st = dc_rest_request_set_path(&req, dc_string_cstr(&path));
    dc_string_free(&path);
    if (st != DC_OK) goto cleanup;

    st = dc_rest_request_set_json_body(&req, json_body);
    if (st != DC_OK) goto cleanup;

    st = dc_rest_execute(client->rest, &req, &resp);
    if (st != DC_OK) goto cleanup;

    if (resp.http.status_code < 200 || resp.http.status_code >= 300) {
        st = dc_status_from_http(resp.http.status_code);
        goto cleanup;
    }

    if (message_id) {
        st = dc_client_parse_message_id(dc_string_cstr(&resp.http.body),
                                        dc_string_length(&resp.http.body),
                                        message_id);
    }

cleanup:
    dc_rest_response_free(&resp);
    dc_rest_request_free(&req);
    return st;
}

dc_status_t dc_client_get_guild_json(dc_client_t* client,
                                     dc_snowflake_t guild_id,
                                     dc_string_t* guild_json) {
    if (!client || !client->rest || !guild_json) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id)) return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    }
    if (st == DC_OK) {
        st = dc_string_set_cstr(guild_json, dc_string_cstr(&resp.http.body));
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild(dc_client_t* client,
                                dc_snowflake_t guild_id,
                                dc_guild_t* guild) {
    if (!guild) return DC_ERROR_NULL_POINTER;

    dc_string_t guild_json;
    dc_status_t st = dc_string_init(&guild_json);
    if (st != DC_OK) return st;

    st = dc_client_get_guild_json(client, guild_id, &guild_json);
    if (st == DC_OK) {
        st = dc_client_parse_guild(dc_string_cstr(&guild_json),
                                   dc_string_length(&guild_json),
                                   guild);
    }

    dc_string_free(&guild_json);
    return st;
}

dc_status_t dc_client_get_guild_channels_json(dc_client_t* client,
                                              dc_snowflake_t guild_id,
                                              dc_string_t* channels_json) {
    if (!client || !client->rest || !channels_json) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id)) return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/channels", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    }
    if (st == DC_OK) {
        st = dc_string_set_cstr(channels_json, dc_string_cstr(&resp.http.body));
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_channels(dc_client_t* client,
                                         dc_snowflake_t guild_id,
                                         dc_channel_list_t* channels) {
    if (!channels) return DC_ERROR_NULL_POINTER;

    dc_string_t channels_json;
    dc_status_t st = dc_string_init(&channels_json);
    if (st != DC_OK) return st;

    st = dc_client_get_guild_channels_json(client, guild_id, &channels_json);
    if (st == DC_OK) {
        st = dc_client_parse_channel_list(dc_string_cstr(&channels_json),
                                          dc_string_length(&channels_json),
                                          channels);
    }

    dc_string_free(&channels_json);
    return st;
}

dc_status_t dc_client_get_channel(dc_client_t* client,
                                  dc_snowflake_t channel_id,
                                  dc_channel_t* channel) {
    if (!client || !client->rest || !channel) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id)) return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, channel_buf, sizeof(channel_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s", channel_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    }
    if (st == DC_OK) {
        st = dc_client_parse_channel(dc_string_cstr(&resp.http.body),
                                     dc_string_length(&resp.http.body),
                                     channel);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_channel_json(dc_client_t* client,
                                          dc_snowflake_t channel_id,
                                          const char* json_body,
                                          dc_channel_t* channel) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id)) return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, channel_buf, sizeof(channel_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) return st;
    st = dc_string_init(&path);
    if (st != DC_OK) {
        dc_rest_response_free(&resp);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s", channel_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body, 0, &resp);
    }
    if (st == DC_OK && channel) {
        st = dc_client_parse_channel(dc_string_cstr(&resp.http.body),
                                     dc_string_length(&resp.http.body),
                                     channel);
    }

    dc_string_free(&path);
    dc_rest_response_free(&resp);
    return st;
}

dc_status_t dc_client_delete_channel(dc_client_t* client,
                                     dc_snowflake_t channel_id,
                                     dc_channel_t* channel) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id)) return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, channel_buf, sizeof(channel_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) return st;
    st = dc_string_init(&path);
    if (st != DC_OK) {
        dc_rest_response_free(&resp);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s", channel_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL, 0, &resp);
    }
    if (st == DC_OK && channel && dc_string_length(&resp.http.body) > 0) {
        st = dc_client_parse_channel(dc_string_cstr(&resp.http.body),
                                     dc_string_length(&resp.http.body),
                                     channel);
    }

    dc_string_free(&path);
    dc_rest_response_free(&resp);
    return st;
}

dc_status_t dc_client_list_channel_messages_json(dc_client_t* client,
                                                 dc_snowflake_t channel_id,
                                                 uint32_t limit,
                                                 dc_snowflake_t before,
                                                 dc_snowflake_t after,
                                                 dc_snowflake_t around,
                                                 dc_string_t* messages_json) {
    if (!client || !client->rest || !messages_json) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id)) return DC_ERROR_INVALID_PARAM;

    int cursor_count = 0;
    cursor_count += dc_snowflake_is_valid(before) ? 1 : 0;
    cursor_count += dc_snowflake_is_valid(after) ? 1 : 0;
    cursor_count += dc_snowflake_is_valid(around) ? 1 : 0;
    if (cursor_count > 1) return DC_ERROR_INVALID_PARAM;
    if (limit > 100u) return DC_ERROR_INVALID_PARAM;
    if (limit == 0u) limit = 50u;

    char channel_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, channel_buf, sizeof(channel_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s/messages?limit=%u", channel_buf, limit);
    if (st != DC_OK) goto cleanup;

    if (dc_snowflake_is_valid(before)) {
        char msg_buf[32];
        st = dc_snowflake_to_cstr(before, msg_buf, sizeof(msg_buf));
        if (st != DC_OK) goto cleanup;
        st = dc_string_append_cstr(&path, "&before=");
        if (st != DC_OK) goto cleanup;
        st = dc_string_append_cstr(&path, msg_buf);
        if (st != DC_OK) goto cleanup;
    } else if (dc_snowflake_is_valid(after)) {
        char msg_buf[32];
        st = dc_snowflake_to_cstr(after, msg_buf, sizeof(msg_buf));
        if (st != DC_OK) goto cleanup;
        st = dc_string_append_cstr(&path, "&after=");
        if (st != DC_OK) goto cleanup;
        st = dc_string_append_cstr(&path, msg_buf);
        if (st != DC_OK) goto cleanup;
    } else if (dc_snowflake_is_valid(around)) {
        char msg_buf[32];
        st = dc_snowflake_to_cstr(around, msg_buf, sizeof(msg_buf));
        if (st != DC_OK) goto cleanup;
        st = dc_string_append_cstr(&path, "&around=");
        if (st != DC_OK) goto cleanup;
        st = dc_string_append_cstr(&path, msg_buf);
        if (st != DC_OK) goto cleanup;
    }

    st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    if (st != DC_OK) goto cleanup;

    st = dc_string_set_cstr(messages_json, dc_string_cstr(&resp.http.body));

cleanup:
    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_message(dc_client_t* client,
                                  dc_snowflake_t channel_id,
                                  dc_snowflake_t message_id,
                                  dc_message_t* message) {
    if (!client || !client->rest || !message) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id) || !dc_snowflake_is_valid(message_id)) {
        return DC_ERROR_INVALID_PARAM;
    }

    char channel_buf[32];
    char message_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, channel_buf, sizeof(channel_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(message_id, message_buf, sizeof(message_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s/messages/%s", channel_buf, message_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    }
    if (st == DC_OK) {
        st = dc_client_parse_message(dc_string_cstr(&resp.http.body),
                                     dc_string_length(&resp.http.body),
                                     message);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_edit_message_content(dc_client_t* client,
                                           dc_snowflake_t channel_id,
                                           dc_snowflake_t message_id,
                                           const char* content,
                                           dc_message_t* message) {
    if (!client || !client->rest || !content) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id) || !dc_snowflake_is_valid(message_id)) {
        return DC_ERROR_INVALID_PARAM;
    }

    char channel_buf[32];
    char message_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, channel_buf, sizeof(channel_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(message_id, message_buf, sizeof(message_buf));
    if (st != DC_OK) return st;

    dc_json_mut_doc_t doc;
    st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;
    st = dc_json_mut_set_string(&doc, doc.root, "content", content);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }

    dc_string_t json_body;
    st = dc_string_init(&json_body);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }
    st = dc_json_mut_doc_serialize(&doc, &json_body);
    dc_json_mut_doc_free(&doc);
    if (st != DC_OK) {
        dc_string_free(&json_body);
        return st;
    }

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) {
        dc_string_free(&json_body);
        return st;
    }
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        dc_string_free(&json_body);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s/messages/%s", channel_buf, message_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_PATCH, dc_string_cstr(&path),
                                            dc_string_cstr(&json_body), 0, &resp);
    }
    if (st == DC_OK && message) {
        st = dc_client_parse_message(dc_string_cstr(&resp.http.body),
                                     dc_string_length(&resp.http.body),
                                     message);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    dc_string_free(&json_body);
    return st;
}

dc_status_t dc_client_delete_message(dc_client_t* client,
                                     dc_snowflake_t channel_id,
                                     dc_snowflake_t message_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id) || !dc_snowflake_is_valid(message_id)) {
        return DC_ERROR_INVALID_PARAM;
    }

    char channel_buf[32];
    char message_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, channel_buf, sizeof(channel_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(message_id, message_buf, sizeof(message_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s/messages/%s", channel_buf, message_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL, 0, &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_pin_message(dc_client_t* client,
                                  dc_snowflake_t channel_id,
                                  dc_snowflake_t message_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id) || !dc_snowflake_is_valid(message_id)) {
        return DC_ERROR_INVALID_PARAM;
    }

    char channel_buf[32];
    char message_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, channel_buf, sizeof(channel_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(message_id, message_buf, sizeof(message_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s/pins/%s", channel_buf, message_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_PUT, dc_string_cstr(&path), NULL, 0, &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_unpin_message(dc_client_t* client,
                                    dc_snowflake_t channel_id,
                                    dc_snowflake_t message_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id) || !dc_snowflake_is_valid(message_id)) {
        return DC_ERROR_INVALID_PARAM;
    }

    char channel_buf[32];
    char message_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, channel_buf, sizeof(channel_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(message_id, message_buf, sizeof(message_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s/pins/%s", channel_buf, message_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL, 0, &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_pinned_messages_json(dc_client_t* client,
                                               dc_snowflake_t channel_id,
                                               dc_string_t* messages_json) {
    if (!client || !client->rest || !messages_json) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id)) return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, channel_buf, sizeof(channel_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s/pins", channel_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    }
    if (st == DC_OK) {
        st = dc_string_set_cstr(messages_json, dc_string_cstr(&resp.http.body));
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_guild_json(dc_client_t* client,
                                        dc_snowflake_t guild_id,
                                        const char* json_body,
                                        dc_string_t* guild_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id)) return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body, 0, &resp);
    }
    if (st == DC_OK && guild_json) {
        st = dc_client_response_body_to_json_out(&resp, guild_json);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_guild(dc_client_t* client,
                                   dc_snowflake_t guild_id,
                                   const char* json_body,
                                   dc_guild_t* guild) {
    if (!guild) {
        return dc_client_modify_guild_json(client, guild_id, json_body, NULL);
    }

    dc_string_t guild_json;
    dc_status_t st = dc_string_init(&guild_json);
    if (st != DC_OK) return st;

    st = dc_client_modify_guild_json(client, guild_id, json_body, &guild_json);
    if (st == DC_OK) {
        st = dc_client_parse_guild(dc_string_cstr(&guild_json),
                                   dc_string_length(&guild_json),
                                   guild);
    }

    dc_string_free(&guild_json);
    return st;
}

dc_status_t dc_client_create_guild_channel_json(dc_client_t* client,
                                                dc_snowflake_t guild_id,
                                                const char* json_body,
                                                dc_channel_t* channel) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id)) return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/channels", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_POST, dc_string_cstr(&path), json_body, 0, &resp);
    }
    if (st == DC_OK && channel) {
        st = dc_client_parse_channel(dc_string_cstr(&resp.http.body),
                                     dc_string_length(&resp.http.body),
                                     channel);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_member_json(dc_client_t* client,
                                            dc_snowflake_t guild_id,
                                            dc_snowflake_t user_id,
                                            dc_string_t* member_json) {
    if (!client || !client->rest || !member_json) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id) || !dc_snowflake_is_valid(user_id)) {
        return DC_ERROR_INVALID_PARAM;
    }

    char guild_buf[32];
    char user_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(user_id, user_buf, sizeof(user_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/members/%s", guild_buf, user_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    }
    if (st == DC_OK) {
        st = dc_client_response_body_to_json_out(&resp, member_json);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_member(dc_client_t* client,
                                       dc_snowflake_t guild_id,
                                       dc_snowflake_t user_id,
                                       dc_guild_member_t* member) {
    if (!member) return DC_ERROR_NULL_POINTER;

    dc_string_t member_json;
    dc_status_t st = dc_string_init(&member_json);
    if (st != DC_OK) return st;

    st = dc_client_get_guild_member_json(client, guild_id, user_id, &member_json);
    if (st == DC_OK) {
        st = dc_client_parse_guild_member(dc_string_cstr(&member_json),
                                          dc_string_length(&member_json),
                                          member);
    }

    dc_string_free(&member_json);
    return st;
}

dc_status_t dc_client_list_guild_members_json(dc_client_t* client,
                                              dc_snowflake_t guild_id,
                                              uint32_t limit,
                                              dc_snowflake_t after,
                                              dc_string_t* members_json) {
    if (!client || !client->rest || !members_json) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id)) return DC_ERROR_INVALID_PARAM;
    if (limit == 0u) limit = 1u;
    if (limit > 1000u) return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/members?limit=%u", guild_buf, limit);
    if (st != DC_OK) goto cleanup_list_members;

    if (dc_snowflake_is_valid(after)) {
        char after_buf[32];
        st = dc_snowflake_to_cstr(after, after_buf, sizeof(after_buf));
        if (st != DC_OK) goto cleanup_list_members;
        st = dc_string_append_cstr(&path, "&after=");
        if (st != DC_OK) goto cleanup_list_members;
        st = dc_string_append_cstr(&path, after_buf);
        if (st != DC_OK) goto cleanup_list_members;
    }

    st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    if (st != DC_OK) goto cleanup_list_members;

    st = dc_client_response_body_to_json_out(&resp, members_json);

cleanup_list_members:
    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_list_guild_members(dc_client_t* client,
                                         dc_snowflake_t guild_id,
                                         uint32_t limit,
                                         dc_snowflake_t after,
                                         dc_guild_member_list_t* members) {
    if (!members) return DC_ERROR_NULL_POINTER;

    dc_string_t members_json;
    dc_status_t st = dc_string_init(&members_json);
    if (st != DC_OK) return st;

    st = dc_client_list_guild_members_json(client, guild_id, limit, after, &members_json);
    if (st == DC_OK) {
        st = dc_client_parse_guild_member_list(dc_string_cstr(&members_json),
                                               dc_string_length(&members_json),
                                               members);
    }

    dc_string_free(&members_json);
    return st;
}

dc_status_t dc_client_get_guild_roles_json(dc_client_t* client,
                                           dc_snowflake_t guild_id,
                                           dc_string_t* roles_json) {
    if (!client || !client->rest || !roles_json) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id)) return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/roles", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    }
    if (st == DC_OK) {
        st = dc_client_response_body_to_json_out(&resp, roles_json);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_roles(dc_client_t* client,
                                      dc_snowflake_t guild_id,
                                      dc_role_list_t* roles) {
    if (!roles) return DC_ERROR_NULL_POINTER;

    dc_string_t roles_json;
    dc_status_t st = dc_string_init(&roles_json);
    if (st != DC_OK) return st;

    st = dc_client_get_guild_roles_json(client, guild_id, &roles_json);
    if (st == DC_OK) {
        st = dc_client_parse_role_list(dc_string_cstr(&roles_json),
                                       dc_string_length(&roles_json),
                                       roles);
    }

    dc_string_free(&roles_json);
    return st;
}

dc_status_t dc_client_create_guild_role_json(dc_client_t* client,
                                             dc_snowflake_t guild_id,
                                             const char* json_body,
                                             dc_role_t* role) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id)) return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/roles", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_POST, dc_string_cstr(&path), json_body, 0, &resp);
    }
    if (st == DC_OK && role) {
        st = dc_client_parse_role(dc_string_cstr(&resp.http.body),
                                  dc_string_length(&resp.http.body),
                                  role);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_guild_role_positions_json(dc_client_t* client,
                                                       dc_snowflake_t guild_id,
                                                       const char* json_body,
                                                       dc_role_list_t* roles) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id)) return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/roles", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body, 0, &resp);
    }
    if (st == DC_OK && roles) {
        st = dc_client_parse_role_list(dc_string_cstr(&resp.http.body),
                                       dc_string_length(&resp.http.body),
                                       roles);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_guild_role_json(dc_client_t* client,
                                             dc_snowflake_t guild_id,
                                             dc_snowflake_t role_id,
                                             const char* json_body,
                                             dc_role_t* role) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id) || !dc_snowflake_is_valid(role_id)) {
        return DC_ERROR_INVALID_PARAM;
    }

    char guild_buf[32];
    char role_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(role_id, role_buf, sizeof(role_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/roles/%s", guild_buf, role_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body, 0, &resp);
    }
    if (st == DC_OK && role) {
        st = dc_client_parse_role(dc_string_cstr(&resp.http.body),
                                  dc_string_length(&resp.http.body),
                                  role);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_delete_guild_role(dc_client_t* client,
                                        dc_snowflake_t guild_id,
                                        dc_snowflake_t role_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id) || !dc_snowflake_is_valid(role_id)) {
        return DC_ERROR_INVALID_PARAM;
    }

    char guild_buf[32];
    char role_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(role_id, role_buf, sizeof(role_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/roles/%s", guild_buf, role_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL, 0, &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_remove_guild_member(dc_client_t* client,
                                          dc_snowflake_t guild_id,
                                          dc_snowflake_t user_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id) || !dc_snowflake_is_valid(user_id)) {
        return DC_ERROR_INVALID_PARAM;
    }

    char guild_buf[32];
    char user_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(user_id, user_buf, sizeof(user_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/members/%s", guild_buf, user_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL, 0, &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_add_guild_member_role(dc_client_t* client,
                                            dc_snowflake_t guild_id,
                                            dc_snowflake_t user_id,
                                            dc_snowflake_t role_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id) || !dc_snowflake_is_valid(user_id) ||
        !dc_snowflake_is_valid(role_id)) {
        return DC_ERROR_INVALID_PARAM;
    }

    char guild_buf[32];
    char user_buf[32];
    char role_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(user_id, user_buf, sizeof(user_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(role_id, role_buf, sizeof(role_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/members/%s/roles/%s", guild_buf, user_buf, role_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_PUT, dc_string_cstr(&path), NULL, 0, &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_remove_guild_member_role(dc_client_t* client,
                                               dc_snowflake_t guild_id,
                                               dc_snowflake_t user_id,
                                               dc_snowflake_t role_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id) || !dc_snowflake_is_valid(user_id) ||
        !dc_snowflake_is_valid(role_id)) {
        return DC_ERROR_INVALID_PARAM;
    }

    char guild_buf[32];
    char user_buf[32];
    char role_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(user_id, user_buf, sizeof(user_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(role_id, role_buf, sizeof(role_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/members/%s/roles/%s", guild_buf, user_buf, role_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL, 0, &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_create_channel_webhook_json(dc_client_t* client,
                                                  dc_snowflake_t channel_id,
                                                  const char* json_body,
                                                  dc_string_t* webhook_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id)) return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, channel_buf, sizeof(channel_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s/webhooks", channel_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_POST, dc_string_cstr(&path), json_body, 0, &resp);
    }
    if (st == DC_OK && webhook_json) {
        st = dc_client_response_body_to_json_out(&resp, webhook_json);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_channel_webhooks_json(dc_client_t* client,
                                                dc_snowflake_t channel_id,
                                                dc_string_t* webhooks_json) {
    if (!client || !client->rest || !webhooks_json) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(channel_id)) return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(channel_id, channel_buf, sizeof(channel_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s/webhooks", channel_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    }
    if (st == DC_OK) {
        st = dc_client_response_body_to_json_out(&resp, webhooks_json);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_webhooks_json(dc_client_t* client,
                                              dc_snowflake_t guild_id,
                                              dc_string_t* webhooks_json) {
    if (!client || !client->rest || !webhooks_json) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(guild_id)) return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/guilds/%s/webhooks", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    }
    if (st == DC_OK) {
        st = dc_client_response_body_to_json_out(&resp, webhooks_json);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_webhook_json(dc_client_t* client,
                                       dc_snowflake_t webhook_id,
                                       dc_string_t* webhook_json) {
    if (!client || !client->rest || !webhook_json) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(webhook_id)) return DC_ERROR_INVALID_PARAM;

    char webhook_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(webhook_id, webhook_buf, sizeof(webhook_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s", webhook_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    }
    if (st == DC_OK) {
        st = dc_client_response_body_to_json_out(&resp, webhook_json);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_webhook_with_token_json(dc_client_t* client,
                                                  dc_snowflake_t webhook_id,
                                                  const char* webhook_token,
                                                  dc_string_t* webhook_json) {
    if (!client || !client->rest || !webhook_token || !webhook_json) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(webhook_id) || webhook_token[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char webhook_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(webhook_id, webhook_buf, sizeof(webhook_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s/%s", webhook_buf, webhook_token);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    }
    if (st == DC_OK) {
        st = dc_client_response_body_to_json_out(&resp, webhook_json);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_webhook_json(dc_client_t* client,
                                          dc_snowflake_t webhook_id,
                                          const char* json_body,
                                          dc_string_t* webhook_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(webhook_id)) return DC_ERROR_INVALID_PARAM;

    char webhook_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(webhook_id, webhook_buf, sizeof(webhook_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s", webhook_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body, 0, &resp);
    }
    if (st == DC_OK && webhook_json) {
        st = dc_client_response_body_to_json_out(&resp, webhook_json);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_webhook_with_token_json(dc_client_t* client,
                                                     dc_snowflake_t webhook_id,
                                                     const char* webhook_token,
                                                     const char* json_body,
                                                     dc_string_t* webhook_json) {
    if (!client || !client->rest || !webhook_token || !json_body) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(webhook_id) || webhook_token[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char webhook_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(webhook_id, webhook_buf, sizeof(webhook_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s/%s", webhook_buf, webhook_token);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body, 0, &resp);
    }
    if (st == DC_OK && webhook_json) {
        st = dc_client_response_body_to_json_out(&resp, webhook_json);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_delete_webhook(dc_client_t* client,
                                     dc_snowflake_t webhook_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(webhook_id)) return DC_ERROR_INVALID_PARAM;

    char webhook_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(webhook_id, webhook_buf, sizeof(webhook_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s", webhook_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL, 0, &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_delete_webhook_with_token(dc_client_t* client,
                                                dc_snowflake_t webhook_id,
                                                const char* webhook_token) {
    if (!client || !client->rest || !webhook_token) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(webhook_id) || webhook_token[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char webhook_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(webhook_id, webhook_buf, sizeof(webhook_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s/%s", webhook_buf, webhook_token);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL, 0, &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_execute_webhook_json(dc_client_t* client,
                                           dc_snowflake_t webhook_id,
                                           const char* webhook_token,
                                           const char* json_body,
                                           int wait,
                                           dc_string_t* message_json) {
    if (!client || !client->rest || !webhook_token || !json_body) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(webhook_id) || webhook_token[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char webhook_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(webhook_id, webhook_buf, sizeof(webhook_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s/%s?wait=%s",
                          webhook_buf, webhook_token, wait ? "true" : "false");
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_POST, dc_string_cstr(&path), json_body, 0, &resp);
    }
    if (st == DC_OK && message_json) {
        if (wait) {
            st = dc_client_response_body_to_json_out(&resp, message_json);
        } else {
            st = dc_string_clear(message_json);
        }
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_webhook_message_json(dc_client_t* client,
                                               dc_snowflake_t webhook_id,
                                               const char* webhook_token,
                                               dc_snowflake_t message_id,
                                               dc_snowflake_t thread_id,
                                               dc_string_t* message_json) {
    if (!client || !client->rest || !webhook_token || !message_json) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(webhook_id) || !dc_snowflake_is_valid(message_id) || webhook_token[0] == '\0') {
        return DC_ERROR_INVALID_PARAM;
    }

    char webhook_buf[32];
    char message_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(webhook_id, webhook_buf, sizeof(webhook_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(message_id, message_buf, sizeof(message_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s/%s/messages/%s", webhook_buf, webhook_token, message_buf);
    if (st != DC_OK) goto cleanup_get_webhook_msg;

    if (dc_snowflake_is_valid(thread_id)) {
        char thread_buf[32];
        st = dc_snowflake_to_cstr(thread_id, thread_buf, sizeof(thread_buf));
        if (st != DC_OK) goto cleanup_get_webhook_msg;
        st = dc_string_append_cstr(&path, "?thread_id=");
        if (st != DC_OK) goto cleanup_get_webhook_msg;
        st = dc_string_append_cstr(&path, thread_buf);
        if (st != DC_OK) goto cleanup_get_webhook_msg;
    }

    st = dc_client_execute_json_request(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0, &resp);
    if (st != DC_OK) goto cleanup_get_webhook_msg;
    st = dc_client_response_body_to_json_out(&resp, message_json);

cleanup_get_webhook_msg:
    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_edit_webhook_message_json(dc_client_t* client,
                                                dc_snowflake_t webhook_id,
                                                const char* webhook_token,
                                                dc_snowflake_t message_id,
                                                const char* json_body,
                                                dc_snowflake_t thread_id,
                                                dc_string_t* message_json) {
    if (!client || !client->rest || !webhook_token || !json_body) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(webhook_id) || !dc_snowflake_is_valid(message_id) || webhook_token[0] == '\0') {
        return DC_ERROR_INVALID_PARAM;
    }

    char webhook_buf[32];
    char message_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(webhook_id, webhook_buf, sizeof(webhook_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(message_id, message_buf, sizeof(message_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s/%s/messages/%s", webhook_buf, webhook_token, message_buf);
    if (st != DC_OK) goto cleanup_edit_webhook_msg;

    if (dc_snowflake_is_valid(thread_id)) {
        char thread_buf[32];
        st = dc_snowflake_to_cstr(thread_id, thread_buf, sizeof(thread_buf));
        if (st != DC_OK) goto cleanup_edit_webhook_msg;
        st = dc_string_append_cstr(&path, "?thread_id=");
        if (st != DC_OK) goto cleanup_edit_webhook_msg;
        st = dc_string_append_cstr(&path, thread_buf);
        if (st != DC_OK) goto cleanup_edit_webhook_msg;
    }

    st = dc_client_execute_json_request(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body, 0, &resp);
    if (st != DC_OK) goto cleanup_edit_webhook_msg;
    if (message_json) {
        st = dc_client_response_body_to_json_out(&resp, message_json);
    }

cleanup_edit_webhook_msg:
    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_delete_webhook_message(dc_client_t* client,
                                             dc_snowflake_t webhook_id,
                                             const char* webhook_token,
                                             dc_snowflake_t message_id,
                                             dc_snowflake_t thread_id) {
    if (!client || !client->rest || !webhook_token) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(webhook_id) || !dc_snowflake_is_valid(message_id) || webhook_token[0] == '\0') {
        return DC_ERROR_INVALID_PARAM;
    }

    char webhook_buf[32];
    char message_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(webhook_id, webhook_buf, sizeof(webhook_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(message_id, message_buf, sizeof(message_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s/%s/messages/%s", webhook_buf, webhook_token, message_buf);
    if (st != DC_OK) goto cleanup_delete_webhook_msg;

    if (dc_snowflake_is_valid(thread_id)) {
        char thread_buf[32];
        st = dc_snowflake_to_cstr(thread_id, thread_buf, sizeof(thread_buf));
        if (st != DC_OK) goto cleanup_delete_webhook_msg;
        st = dc_string_append_cstr(&path, "?thread_id=");
        if (st != DC_OK) goto cleanup_delete_webhook_msg;
        st = dc_string_append_cstr(&path, thread_buf);
        if (st != DC_OK) goto cleanup_delete_webhook_msg;
    }

    st = dc_client_execute_json_request(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL, 0, &resp);

cleanup_delete_webhook_msg:
    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_current_application_json(dc_client_t* client,
                                                   dc_string_t* application_json) {
    if (!client || !client->rest || !application_json) return DC_ERROR_NULL_POINTER;
    return dc_client_execute_json_request_out(client, DC_HTTP_GET, "/applications/@me", NULL, 0,
                                              application_json);
}

dc_status_t dc_client_modify_current_application_json(dc_client_t* client,
                                                      const char* json_body,
                                                      dc_string_t* application_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;
    return dc_client_execute_json_request_out(client, DC_HTTP_PATCH, "/applications/@me", json_body,
                                              0, application_json);
}

dc_status_t dc_client_get_application_role_connection_metadata_json(dc_client_t* client,
                                                                    dc_snowflake_t application_id,
                                                                    dc_string_t* metadata_json) {
    if (!client || !client->rest || !metadata_json) return DC_ERROR_NULL_POINTER;
    char app_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/role-connections/metadata", app_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                metadata_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_update_application_role_connection_metadata_json(dc_client_t* client,
                                                                       dc_snowflake_t application_id,
                                                                       const char* json_body,
                                                                       dc_string_t* metadata_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char app_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/role-connections/metadata", app_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PUT, dc_string_cstr(&path), json_body,
                                                0, metadata_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_global_application_commands_json(dc_client_t* client,
                                                           dc_snowflake_t application_id,
                                                           int with_localizations,
                                                           dc_string_t* commands_json) {
    if (!client || !client->rest || !commands_json) return DC_ERROR_NULL_POINTER;

    char app_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/commands", app_buf);
    if (st == DC_OK && with_localizations) {
        st = dc_http_append_query_bool(&path, "with_localizations", 1, DC_HTTP_BOOL_TRUE_FALSE);
    }
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                commands_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_create_global_application_command_json(dc_client_t* client,
                                                             dc_snowflake_t application_id,
                                                             const char* json_body,
                                                             dc_string_t* command_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char app_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/commands", app_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_POST, dc_string_cstr(&path), json_body,
                                                0, command_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_global_application_command_json(dc_client_t* client,
                                                          dc_snowflake_t application_id,
                                                          dc_snowflake_t command_id,
                                                          dc_string_t* command_json) {
    if (!client || !client->rest || !command_json) return DC_ERROR_NULL_POINTER;

    char app_buf[32];
    char cmd_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(command_id, cmd_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/commands/%s", app_buf, cmd_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                command_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_global_application_command_json(dc_client_t* client,
                                                             dc_snowflake_t application_id,
                                                             dc_snowflake_t command_id,
                                                             const char* json_body,
                                                             dc_string_t* command_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char app_buf[32];
    char cmd_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(command_id, cmd_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/commands/%s", app_buf, cmd_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body,
                                                0, command_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_delete_global_application_command(dc_client_t* client,
                                                        dc_snowflake_t application_id,
                                                        dc_snowflake_t command_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;

    char app_buf[32];
    char cmd_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(command_id, cmd_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/commands/%s", app_buf, cmd_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL,
                                                0, NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_bulk_overwrite_global_application_commands_json(dc_client_t* client,
                                                                      dc_snowflake_t application_id,
                                                                      const char* json_body,
                                                                      dc_string_t* commands_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char app_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/commands", app_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PUT, dc_string_cstr(&path), json_body,
                                                0, commands_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_application_commands_json(dc_client_t* client,
                                                          dc_snowflake_t application_id,
                                                          dc_snowflake_t guild_id,
                                                          int with_localizations,
                                                          dc_string_t* commands_json) {
    if (!client || !client->rest || !commands_json) return DC_ERROR_NULL_POINTER;

    char app_buf[32];
    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/guilds/%s/commands", app_buf, guild_buf);
    if (st == DC_OK && with_localizations) {
        st = dc_http_append_query_bool(&path, "with_localizations", 1, DC_HTTP_BOOL_TRUE_FALSE);
    }
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                commands_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_create_guild_application_command_json(dc_client_t* client,
                                                            dc_snowflake_t application_id,
                                                            dc_snowflake_t guild_id,
                                                            const char* json_body,
                                                            dc_string_t* command_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char app_buf[32];
    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/guilds/%s/commands", app_buf, guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_POST, dc_string_cstr(&path), json_body,
                                                0, command_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_application_command_json(dc_client_t* client,
                                                         dc_snowflake_t application_id,
                                                         dc_snowflake_t guild_id,
                                                         dc_snowflake_t command_id,
                                                         dc_string_t* command_json) {
    if (!client || !client->rest || !command_json) return DC_ERROR_NULL_POINTER;

    char app_buf[32];
    char guild_buf[32];
    char cmd_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(command_id, cmd_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/guilds/%s/commands/%s", app_buf, guild_buf, cmd_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                command_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_guild_application_command_json(dc_client_t* client,
                                                            dc_snowflake_t application_id,
                                                            dc_snowflake_t guild_id,
                                                            dc_snowflake_t command_id,
                                                            const char* json_body,
                                                            dc_string_t* command_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char app_buf[32];
    char guild_buf[32];
    char cmd_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(command_id, cmd_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/guilds/%s/commands/%s", app_buf, guild_buf, cmd_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body,
                                                0, command_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_delete_guild_application_command(dc_client_t* client,
                                                       dc_snowflake_t application_id,
                                                       dc_snowflake_t guild_id,
                                                       dc_snowflake_t command_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;

    char app_buf[32];
    char guild_buf[32];
    char cmd_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(command_id, cmd_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/guilds/%s/commands/%s", app_buf, guild_buf, cmd_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL,
                                                0, NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_bulk_overwrite_guild_application_commands_json(dc_client_t* client,
                                                                     dc_snowflake_t application_id,
                                                                     dc_snowflake_t guild_id,
                                                                     const char* json_body,
                                                                     dc_string_t* commands_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char app_buf[32];
    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/guilds/%s/commands", app_buf, guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PUT, dc_string_cstr(&path), json_body,
                                                0, commands_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_application_command_permissions_json(dc_client_t* client,
                                                                     dc_snowflake_t application_id,
                                                                     dc_snowflake_t guild_id,
                                                                     dc_string_t* permissions_json) {
    if (!client || !client->rest || !permissions_json) return DC_ERROR_NULL_POINTER;

    char app_buf[32];
    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/guilds/%s/commands/permissions", app_buf, guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                permissions_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_application_command_permissions_json(dc_client_t* client,
                                                               dc_snowflake_t application_id,
                                                               dc_snowflake_t guild_id,
                                                               dc_snowflake_t command_id,
                                                               dc_string_t* permission_json) {
    if (!client || !client->rest || !permission_json) return DC_ERROR_NULL_POINTER;

    char app_buf[32];
    char guild_buf[32];
    char cmd_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(command_id, cmd_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/guilds/%s/commands/%s/permissions",
                          app_buf, guild_buf, cmd_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                permission_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_edit_application_command_permissions_json(dc_client_t* client,
                                                                dc_snowflake_t application_id,
                                                                dc_snowflake_t guild_id,
                                                                dc_snowflake_t command_id,
                                                                const char* json_body,
                                                                dc_string_t* permission_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char app_buf[32];
    char guild_buf[32];
    char cmd_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(command_id, cmd_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/guilds/%s/commands/%s/permissions",
                          app_buf, guild_buf, cmd_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PUT, dc_string_cstr(&path), json_body,
                                                0, permission_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_batch_edit_application_command_permissions_json(dc_client_t* client,
                                                                      dc_snowflake_t application_id,
                                                                      dc_snowflake_t guild_id,
                                                                      const char* json_body,
                                                                      dc_string_t* permissions_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char app_buf[32];
    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(application_id, app_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/applications/%s/guilds/%s/commands/permissions", app_buf, guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PUT, dc_string_cstr(&path), json_body,
                                                0, permissions_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_crosspost_message_json(dc_client_t* client,
                                             dc_snowflake_t channel_id,
                                             dc_snowflake_t message_id,
                                             dc_string_t* message_json) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;

    char channel_buf[32];
    char message_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(message_id, message_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/messages/%s/crosspost", channel_buf, message_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_POST, dc_string_cstr(&path), NULL, 0,
                                                message_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_edit_message_json(dc_client_t* client,
                                        dc_snowflake_t channel_id,
                                        dc_snowflake_t message_id,
                                        const char* json_body,
                                        dc_message_t* message) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    char message_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(message_id, message_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/channels/%s/messages/%s", channel_buf, message_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body, 0,
                                            &resp);
    }
    if (st == DC_OK && message) {
        st = dc_client_parse_message(dc_string_cstr(&resp.http.body),
                                     dc_string_length(&resp.http.body), message);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_bulk_delete_messages_json(dc_client_t* client,
                                                dc_snowflake_t channel_id,
                                                const char* json_body) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/messages/bulk-delete", channel_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_POST, dc_string_cstr(&path), json_body,
                                                0, NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_create_reaction_encoded(dc_client_t* client,
                                              dc_snowflake_t channel_id,
                                              dc_snowflake_t message_id,
                                              const char* emoji_encoded) {
    if (!client || !client->rest || !emoji_encoded) return DC_ERROR_NULL_POINTER;
    if (emoji_encoded[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    char message_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(message_id, message_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/messages/%s/reactions/%s/@me",
                          channel_buf, message_buf, emoji_encoded);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PUT, dc_string_cstr(&path), NULL, 0,
                                                NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_delete_own_reaction_encoded(dc_client_t* client,
                                                  dc_snowflake_t channel_id,
                                                  dc_snowflake_t message_id,
                                                  const char* emoji_encoded) {
    if (!client || !client->rest || !emoji_encoded) return DC_ERROR_NULL_POINTER;
    if (emoji_encoded[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    char message_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(message_id, message_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/messages/%s/reactions/%s/@me",
                          channel_buf, message_buf, emoji_encoded);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL,
                                                0, NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_delete_user_reaction_encoded(dc_client_t* client,
                                                   dc_snowflake_t channel_id,
                                                   dc_snowflake_t message_id,
                                                   const char* emoji_encoded,
                                                   dc_snowflake_t user_id) {
    if (!client || !client->rest || !emoji_encoded) return DC_ERROR_NULL_POINTER;
    if (emoji_encoded[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    char message_buf[32];
    char user_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(message_id, message_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(user_id, user_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/messages/%s/reactions/%s/%s",
                          channel_buf, message_buf, emoji_encoded, user_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL,
                                                0, NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_reactions_encoded_json(dc_client_t* client,
                                                 dc_snowflake_t channel_id,
                                                 dc_snowflake_t message_id,
                                                 const char* emoji_encoded,
                                                 int reaction_type,
                                                 dc_snowflake_t after,
                                                 uint32_t limit,
                                                 dc_string_t* users_json) {
    if (!client || !client->rest || !emoji_encoded || !users_json) return DC_ERROR_NULL_POINTER;
    if (emoji_encoded[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (reaction_type != 0 && reaction_type != 1) return DC_ERROR_INVALID_PARAM;
    if (limit > 100u) return DC_ERROR_INVALID_PARAM;
    if (limit == 0u) limit = 25u;

    char channel_buf[32];
    char message_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(message_id, message_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/messages/%s/reactions/%s?type=%d&limit=%u",
                          channel_buf, message_buf, emoji_encoded, reaction_type, limit);
    if (st == DC_OK && dc_snowflake_is_valid(after)) {
        char after_buf[32];
        st = dc_snowflake_to_cstr(after, after_buf, sizeof(after_buf));
        if (st == DC_OK) st = dc_string_append_cstr(&path, "&after=");
        if (st == DC_OK) st = dc_string_append_cstr(&path, after_buf);
    }
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                users_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_delete_all_reactions(dc_client_t* client,
                                           dc_snowflake_t channel_id,
                                           dc_snowflake_t message_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;

    char channel_buf[32];
    char message_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(message_id, message_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/messages/%s/reactions", channel_buf, message_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL,
                                                0, NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_delete_all_reactions_for_emoji_encoded(dc_client_t* client,
                                                             dc_snowflake_t channel_id,
                                                             dc_snowflake_t message_id,
                                                             const char* emoji_encoded) {
    if (!client || !client->rest || !emoji_encoded) return DC_ERROR_NULL_POINTER;
    if (emoji_encoded[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    char message_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(message_id, message_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/messages/%s/reactions/%s",
                          channel_buf, message_buf, emoji_encoded);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL,
                                                0, NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_channel_pins_json(dc_client_t* client,
                                            dc_snowflake_t channel_id,
                                            const char* before_iso8601,
                                            uint32_t limit,
                                            dc_string_t* pins_json) {
    if (!client || !client->rest || !pins_json) return DC_ERROR_NULL_POINTER;
    if (limit > 50u) return DC_ERROR_INVALID_PARAM;
    if (limit == 0u) limit = 50u;
    if (before_iso8601 && before_iso8601[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/messages/pins?limit=%u", channel_buf, limit);
    if (st == DC_OK && before_iso8601) {
        st = dc_string_append_cstr(&path, "&before=");
        if (st == DC_OK) st = dc_string_append_cstr(&path, before_iso8601);
    }
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                pins_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_edit_channel_permissions_json(dc_client_t* client,
                                                    dc_snowflake_t channel_id,
                                                    dc_snowflake_t overwrite_id,
                                                    const char* json_body) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    char overwrite_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(overwrite_id, overwrite_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/permissions/%s", channel_buf, overwrite_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PUT, dc_string_cstr(&path), json_body,
                                                0, NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_delete_channel_permission(dc_client_t* client,
                                                dc_snowflake_t channel_id,
                                                dc_snowflake_t overwrite_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;

    char channel_buf[32];
    char overwrite_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(overwrite_id, overwrite_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/permissions/%s", channel_buf, overwrite_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL,
                                                0, NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_channel_invites_json(dc_client_t* client,
                                               dc_snowflake_t channel_id,
                                               dc_string_t* invites_json) {
    if (!client || !client->rest || !invites_json) return DC_ERROR_NULL_POINTER;

    char channel_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/invites", channel_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                invites_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_create_channel_invite_json(dc_client_t* client,
                                                 dc_snowflake_t channel_id,
                                                 const char* json_body,
                                                 dc_string_t* invite_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;

    char channel_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/invites", channel_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_POST, dc_string_cstr(&path), json_body,
                                                0, invite_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_follow_announcement_channel_json(dc_client_t* client,
                                                       dc_snowflake_t channel_id,
                                                       const char* json_body,
                                                       dc_string_t* followed_channel_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char channel_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/followers", channel_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_POST, dc_string_cstr(&path), json_body,
                                                0, followed_channel_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_trigger_typing_indicator(dc_client_t* client,
                                               dc_snowflake_t channel_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;

    char channel_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(channel_id, channel_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/channels/%s/typing", channel_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_POST, dc_string_cstr(&path), NULL, 0,
                                                NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_preview_json(dc_client_t* client,
                                             dc_snowflake_t guild_id,
                                             dc_string_t* preview_json) {
    if (!client || !client->rest || !preview_json) return DC_ERROR_NULL_POINTER;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/preview", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                preview_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_guild_channel_positions_json(dc_client_t* client,
                                                          dc_snowflake_t guild_id,
                                                          const char* json_body) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/channels", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body,
                                                0, NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_list_active_guild_threads_json(dc_client_t* client,
                                                     dc_snowflake_t guild_id,
                                                     dc_string_t* threads_json) {
    if (!client || !client->rest || !threads_json) return DC_ERROR_NULL_POINTER;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/threads/active", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                threads_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_search_guild_members_json(dc_client_t* client,
                                                dc_snowflake_t guild_id,
                                                const char* query,
                                                uint32_t limit,
                                                dc_string_t* members_json) {
    if (!client || !client->rest || !query || !members_json) return DC_ERROR_NULL_POINTER;
    if (query[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (limit > 1000u) return DC_ERROR_INVALID_PARAM;
    if (limit == 0u) limit = 1u;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/members/search?query=%s&limit=%u", guild_buf, query, limit);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                members_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_guild_member_json(dc_client_t* client,
                                               dc_snowflake_t guild_id,
                                               dc_snowflake_t user_id,
                                               const char* json_body,
                                               dc_string_t* member_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    char user_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(user_id, user_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/members/%s", guild_buf, user_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body,
                                                0, member_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_current_member_json(dc_client_t* client,
                                                 dc_snowflake_t guild_id,
                                                 const char* json_body,
                                                 dc_string_t* member_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/members/@me", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body,
                                                0, member_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_modify_current_user_nick_json(dc_client_t* client,
                                                    dc_snowflake_t guild_id,
                                                    const char* json_body,
                                                    dc_string_t* nick_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/members/@me/nick", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PATCH, dc_string_cstr(&path), json_body,
                                                0, nick_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_bans_json(dc_client_t* client,
                                          dc_snowflake_t guild_id,
                                          uint32_t limit,
                                          dc_snowflake_t before,
                                          dc_snowflake_t after,
                                          dc_string_t* bans_json) {
    if (!client || !client->rest || !bans_json) return DC_ERROR_NULL_POINTER;
    if (limit > 1000u) return DC_ERROR_INVALID_PARAM;
    if (limit == 0u) limit = 1000u;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/bans?limit=%u", guild_buf, limit);
    if (st == DC_OK && dc_snowflake_is_valid(before)) {
        char before_buf[32];
        st = dc_snowflake_to_cstr(before, before_buf, sizeof(before_buf));
        if (st == DC_OK) st = dc_string_append_cstr(&path, "&before=");
        if (st == DC_OK) st = dc_string_append_cstr(&path, before_buf);
    } else if (st == DC_OK && dc_snowflake_is_valid(after)) {
        char after_buf[32];
        st = dc_snowflake_to_cstr(after, after_buf, sizeof(after_buf));
        if (st == DC_OK) st = dc_string_append_cstr(&path, "&after=");
        if (st == DC_OK) st = dc_string_append_cstr(&path, after_buf);
    }
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                bans_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_ban_json(dc_client_t* client,
                                         dc_snowflake_t guild_id,
                                         dc_snowflake_t user_id,
                                         dc_string_t* ban_json) {
    if (!client || !client->rest || !ban_json) return DC_ERROR_NULL_POINTER;

    char guild_buf[32];
    char user_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(user_id, user_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/bans/%s", guild_buf, user_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                ban_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_create_guild_ban(dc_client_t* client,
                                       dc_snowflake_t guild_id,
                                       dc_snowflake_t user_id,
                                       int delete_message_seconds) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (delete_message_seconds < -1 || delete_message_seconds > 604800) return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    char user_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(user_id, user_buf);
    if (st != DC_OK) return st;

    dc_string_t json_body;
    st = dc_string_init(&json_body);
    if (st != DC_OK) return st;
    if (delete_message_seconds >= 0) {
        st = dc_string_printf(&json_body, "{\"delete_message_seconds\":%d}", delete_message_seconds);
    } else {
        st = dc_string_set_cstr(&json_body, "{}");
    }
    if (st != DC_OK) {
        dc_string_free(&json_body);
        return st;
    }

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) {
        dc_string_free(&json_body);
        return st;
    }
    st = dc_string_printf(&path, "/guilds/%s/bans/%s", guild_buf, user_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_PUT, dc_string_cstr(&path),
                                                dc_string_cstr(&json_body), 0, NULL);
    }

    dc_string_free(&path);
    dc_string_free(&json_body);
    return st;
}

dc_status_t dc_client_remove_guild_ban(dc_client_t* client,
                                       dc_snowflake_t guild_id,
                                       dc_snowflake_t user_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;

    char guild_buf[32];
    char user_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(user_id, user_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/bans/%s", guild_buf, user_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL,
                                                0, NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_bulk_guild_ban_json(dc_client_t* client,
                                          dc_snowflake_t guild_id,
                                          const char* json_body,
                                          dc_string_t* result_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/bulk-ban", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_POST, dc_string_cstr(&path), json_body,
                                                0, result_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_role_json(dc_client_t* client,
                                          dc_snowflake_t guild_id,
                                          dc_snowflake_t role_id,
                                          dc_string_t* role_json) {
    if (!client || !client->rest || !role_json) return DC_ERROR_NULL_POINTER;

    char guild_buf[32];
    char role_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(role_id, role_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/roles/%s", guild_buf, role_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                role_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_role_member_counts_json(dc_client_t* client,
                                                        dc_snowflake_t guild_id,
                                                        dc_string_t* counts_json) {
    if (!client || !client->rest || !counts_json) return DC_ERROR_NULL_POINTER;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/roles/member-counts", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                counts_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_prune_count_json(dc_client_t* client,
                                                 dc_snowflake_t guild_id,
                                                 uint32_t days,
                                                 const char* include_roles_csv,
                                                 dc_string_t* prune_json) {
    if (!client || !client->rest || !prune_json) return DC_ERROR_NULL_POINTER;
    if (days > 30u) return DC_ERROR_INVALID_PARAM;
    if (days == 0u) days = 7u;
    if (include_roles_csv && include_roles_csv[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/prune?days=%u", guild_buf, days);
    if (st == DC_OK && include_roles_csv) {
        st = dc_string_append_cstr(&path, "&include_roles=");
        if (st == DC_OK) st = dc_string_append_cstr(&path, include_roles_csv);
    }
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                prune_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_begin_guild_prune_json(dc_client_t* client,
                                             dc_snowflake_t guild_id,
                                             const char* json_body,
                                             dc_string_t* prune_json) {
    if (!client || !client->rest || !json_body) return DC_ERROR_NULL_POINTER;
    if (json_body[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/prune", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_POST, dc_string_cstr(&path), json_body,
                                                0, prune_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_voice_regions_json(dc_client_t* client,
                                                   dc_snowflake_t guild_id,
                                                   dc_string_t* regions_json) {
    if (!client || !client->rest || !regions_json) return DC_ERROR_NULL_POINTER;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/regions", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                regions_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_invites_json(dc_client_t* client,
                                             dc_snowflake_t guild_id,
                                             dc_string_t* invites_json) {
    if (!client || !client->rest || !invites_json) return DC_ERROR_NULL_POINTER;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/invites", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                invites_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_get_guild_integrations_json(dc_client_t* client,
                                                  dc_snowflake_t guild_id,
                                                  dc_string_t* integrations_json) {
    if (!client || !client->rest || !integrations_json) return DC_ERROR_NULL_POINTER;

    char guild_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/integrations", guild_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_GET, dc_string_cstr(&path), NULL, 0,
                                                integrations_json);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_delete_guild_integration(dc_client_t* client,
                                               dc_snowflake_t guild_id,
                                               dc_snowflake_t integration_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;

    char guild_buf[32];
    char integration_buf[32];
    dc_status_t st = dc_client_snowflake_to_buf(guild_id, guild_buf);
    if (st != DC_OK) return st;
    st = dc_client_snowflake_to_buf(integration_id, integration_buf);
    if (st != DC_OK) return st;

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_printf(&path, "/guilds/%s/integrations/%s", guild_buf, integration_buf);
    if (st == DC_OK) {
        st = dc_client_execute_json_request_out(client, DC_HTTP_DELETE, dc_string_cstr(&path), NULL,
                                                0, NULL);
    }
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_update_presence(dc_client_t* client, const char* status,
                                      const char* activity_name, int activity_type) {
    if (!client || !client->gateway) return DC_ERROR_NULL_POINTER;
    return dc_gateway_client_update_presence(client->gateway, status, activity_name, activity_type);
}

dc_status_t dc_client_create_command_simple(dc_client_t* client,
                                            dc_snowflake_t application_id,
                                            dc_snowflake_t guild_id,
                                            const char* name,
                                            const char* description,
                                            const char* option_name,
                                            const char* option_description,
                                            int option_required) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(application_id)) return DC_ERROR_INVALID_PARAM;
    if (!name || !description || !option_name || !option_description) return DC_ERROR_NULL_POINTER;
    if (name[0] == '\0' || description[0] == '\0') return DC_ERROR_INVALID_PARAM;

    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;

    yyjson_mut_val* root = doc.root;
    yyjson_mut_obj_add_strcpy(doc.doc, root, "name", name);
    yyjson_mut_obj_add_strcpy(doc.doc, root, "description", description);
    yyjson_mut_obj_add_int(doc.doc, root, "type", 1);

    yyjson_mut_val* options = yyjson_mut_arr(doc.doc);
    yyjson_mut_val* opt = yyjson_mut_obj(doc.doc);
    yyjson_mut_obj_add_int(doc.doc, opt, "type", 3);
    yyjson_mut_obj_add_strcpy(doc.doc, opt, "name", option_name);
    yyjson_mut_obj_add_strcpy(doc.doc, opt, "description", option_description);
    yyjson_mut_obj_add_bool(doc.doc, opt, "required", option_required ? 1 : 0);
    yyjson_mut_arr_add_val(options, opt);
    yyjson_mut_obj_add(root, yyjson_mut_strcpy(doc.doc, "options"), options);

    dc_string_t json_body;
    st = dc_string_init(&json_body);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }
    st = dc_json_mut_doc_serialize(&doc, &json_body);
    dc_json_mut_doc_free(&doc);
    if (st != DC_OK) {
        dc_string_free(&json_body);
        return st;
    }

    char app_buf[32];
    st = dc_snowflake_to_cstr(application_id, app_buf, sizeof(app_buf));
    if (st != DC_OK) {
        dc_string_free(&json_body);
        return st;
    }

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) {
        dc_string_free(&json_body);
        return st;
    }

    if (dc_snowflake_is_valid(guild_id)) {
        char guild_buf[32];
        st = dc_snowflake_to_cstr(guild_id, guild_buf, sizeof(guild_buf));
        if (st != DC_OK) {
            dc_string_free(&path);
            dc_string_free(&json_body);
            return st;
        }
        st = dc_string_printf(&path, "/applications/%s/guilds/%s/commands", app_buf, guild_buf);
    } else {
        st = dc_string_printf(&path, "/applications/%s/commands", app_buf);
    }
    if (st != DC_OK) {
        dc_string_free(&path);
        dc_string_free(&json_body);
        return st;
    }

    dc_rest_request_t req;
    dc_rest_response_t resp;
    st = dc_rest_request_init(&req);
    if (st != DC_OK) goto cleanup;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) goto cleanup;

    st = dc_rest_request_set_method(&req, DC_HTTP_POST);
    if (st != DC_OK) goto cleanup;
    st = dc_rest_request_set_path(&req, dc_string_cstr(&path));
    if (st != DC_OK) goto cleanup;
    st = dc_rest_request_set_json_body(&req, dc_string_cstr(&json_body));
    if (st != DC_OK) goto cleanup;

    st = dc_rest_execute(client->rest, &req, &resp);
    if (st != DC_OK) goto cleanup;

    if (resp.http.status_code < 200 || resp.http.status_code >= 300) {
        dc_client_log(client, DC_LOG_WARN, "Create command HTTP %d", resp.http.status_code);
        st = dc_status_from_http(resp.http.status_code);
    }

cleanup:
    dc_rest_response_free(&resp);
    dc_rest_request_free(&req);
    dc_string_free(&path);
    dc_string_free(&json_body);
    return st;
}

dc_status_t dc_client_interaction_respond_message(dc_client_t* client,
                                                  dc_snowflake_t interaction_id,
                                                  const char* interaction_token,
                                                  const char* content,
                                                  int ephemeral) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(interaction_id)) return DC_ERROR_INVALID_PARAM;
    if (!interaction_token || interaction_token[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (!content) return DC_ERROR_NULL_POINTER;

    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;
    yyjson_mut_val* root = doc.root;
    yyjson_mut_obj_add_int(doc.doc, root, "type", 4);

    yyjson_mut_val* data = yyjson_mut_obj(doc.doc);
    yyjson_mut_obj_add(root, yyjson_mut_strcpy(doc.doc, "data"), data);
    yyjson_mut_obj_add_strcpy(doc.doc, data, "content", content);
    if (ephemeral) {
        yyjson_mut_obj_add_int(doc.doc, data, "flags", 64);
    }

    dc_string_t json_body;
    st = dc_string_init(&json_body);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }
    st = dc_json_mut_doc_serialize(&doc, &json_body);
    dc_json_mut_doc_free(&doc);
    if (st != DC_OK) {
        dc_string_free(&json_body);
        return st;
    }

    char id_buf[32];
    st = dc_snowflake_to_cstr(interaction_id, id_buf, sizeof(id_buf));
    if (st != DC_OK) {
        dc_string_free(&json_body);
        return st;
    }

    dc_string_t path;
    st = dc_string_init(&path);
    if (st != DC_OK) {
        dc_string_free(&json_body);
        return st;
    }
    st = dc_string_printf(&path, "/interactions/%s/%s/callback", id_buf, interaction_token);
    if (st != DC_OK) {
        dc_string_free(&path);
        dc_string_free(&json_body);
        return st;
    }

    dc_rest_request_t req;
    dc_rest_response_t resp;
    st = dc_rest_request_init(&req);
    if (st != DC_OK) goto cleanup;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) goto cleanup;

    st = dc_rest_request_set_method(&req, DC_HTTP_POST);
    if (st != DC_OK) goto cleanup;
    st = dc_rest_request_set_path(&req, dc_string_cstr(&path));
    if (st != DC_OK) goto cleanup;
    st = dc_rest_request_set_json_body(&req, dc_string_cstr(&json_body));
    if (st != DC_OK) goto cleanup;
    dc_rest_request_set_interaction(&req, 1);

    st = dc_rest_execute(client->rest, &req, &resp);
    if (st != DC_OK) goto cleanup;

    if (resp.http.status_code < 200 || resp.http.status_code >= 300) {
        dc_client_log(client, DC_LOG_WARN, "Interaction response HTTP %d", resp.http.status_code);
        st = dc_status_from_http(resp.http.status_code);
    }

cleanup:
    dc_rest_response_free(&resp);
    dc_rest_request_free(&req);
    dc_string_free(&path);
    dc_string_free(&json_body);
    return st;
}

dc_status_t dc_client_interaction_edit_original_response(dc_client_t* client,
                                                         dc_snowflake_t application_id,
                                                         const char* interaction_token,
                                                         const char* content) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(application_id)) return DC_ERROR_INVALID_PARAM;
    if (!interaction_token || interaction_token[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (!content) return DC_ERROR_NULL_POINTER;

    char app_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(application_id, app_buf, sizeof(app_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_string_t json_body;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_init(&json_body);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&json_body);
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s/%s/messages/@original", app_buf, interaction_token);
    if (st == DC_OK) {
        st = dc_client_json_content_body(content, 0, &json_body);
    }
    if (st == DC_OK) {
        st = dc_client_interaction_webhook_request(client, DC_HTTP_PATCH,
                                                   dc_string_cstr(&path),
                                                   dc_string_cstr(&json_body),
                                                   &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&json_body);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_interaction_delete_original_response(dc_client_t* client,
                                                           dc_snowflake_t application_id,
                                                           const char* interaction_token) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(application_id)) return DC_ERROR_INVALID_PARAM;
    if (!interaction_token || interaction_token[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char app_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(application_id, app_buf, sizeof(app_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s/%s/messages/@original", app_buf, interaction_token);
    if (st == DC_OK) {
        st = dc_client_interaction_webhook_request(client, DC_HTTP_DELETE,
                                                   dc_string_cstr(&path), NULL, &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_interaction_create_followup_message(dc_client_t* client,
                                                          dc_snowflake_t application_id,
                                                          const char* interaction_token,
                                                          const char* content,
                                                          int ephemeral,
                                                          dc_snowflake_t* message_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(application_id)) return DC_ERROR_INVALID_PARAM;
    if (!interaction_token || interaction_token[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (!content) return DC_ERROR_NULL_POINTER;

    char app_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(application_id, app_buf, sizeof(app_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_string_t json_body;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_init(&json_body);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&json_body);
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s/%s", app_buf, interaction_token);
    if (st == DC_OK) {
        st = dc_client_json_content_body(content, ephemeral, &json_body);
    }
    if (st == DC_OK) {
        st = dc_client_interaction_webhook_request(client, DC_HTTP_POST,
                                                   dc_string_cstr(&path),
                                                   dc_string_cstr(&json_body),
                                                   &resp);
    }
    if (st == DC_OK && message_id) {
        st = dc_client_parse_message_id(dc_string_cstr(&resp.http.body),
                                        dc_string_length(&resp.http.body),
                                        message_id);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&json_body);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_interaction_edit_followup_message(dc_client_t* client,
                                                        dc_snowflake_t application_id,
                                                        const char* interaction_token,
                                                        dc_snowflake_t message_id,
                                                        const char* content) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(application_id) || !dc_snowflake_is_valid(message_id)) {
        return DC_ERROR_INVALID_PARAM;
    }
    if (!interaction_token || interaction_token[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (!content) return DC_ERROR_NULL_POINTER;

    char app_buf[32];
    char msg_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(application_id, app_buf, sizeof(app_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(message_id, msg_buf, sizeof(msg_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_string_t json_body;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_string_init(&json_body);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&json_body);
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s/%s/messages/%s", app_buf, interaction_token, msg_buf);
    if (st == DC_OK) {
        st = dc_client_json_content_body(content, 0, &json_body);
    }
    if (st == DC_OK) {
        st = dc_client_interaction_webhook_request(client, DC_HTTP_PATCH,
                                                   dc_string_cstr(&path),
                                                   dc_string_cstr(&json_body),
                                                   &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&json_body);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_interaction_delete_followup_message(dc_client_t* client,
                                                          dc_snowflake_t application_id,
                                                          const char* interaction_token,
                                                          dc_snowflake_t message_id) {
    if (!client || !client->rest) return DC_ERROR_NULL_POINTER;
    if (!dc_snowflake_is_valid(application_id) || !dc_snowflake_is_valid(message_id)) {
        return DC_ERROR_INVALID_PARAM;
    }
    if (!interaction_token || interaction_token[0] == '\0') return DC_ERROR_INVALID_PARAM;

    char app_buf[32];
    char msg_buf[32];
    dc_status_t st = dc_snowflake_to_cstr(application_id, app_buf, sizeof(app_buf));
    if (st != DC_OK) return st;
    st = dc_snowflake_to_cstr(message_id, msg_buf, sizeof(msg_buf));
    if (st != DC_OK) return st;

    dc_string_t path;
    dc_rest_response_t resp;
    st = dc_string_init(&path);
    if (st != DC_OK) return st;
    st = dc_rest_response_init(&resp);
    if (st != DC_OK) {
        dc_string_free(&path);
        return st;
    }

    st = dc_string_printf(&path, "/webhooks/%s/%s/messages/%s", app_buf, interaction_token, msg_buf);
    if (st == DC_OK) {
        st = dc_client_interaction_webhook_request(client, DC_HTTP_DELETE,
                                                   dc_string_cstr(&path), NULL, &resp);
    }

    dc_rest_response_free(&resp);
    dc_string_free(&path);
    return st;
}

dc_status_t dc_client_request_guild_members(dc_client_t* client,
                                            dc_snowflake_t guild_id,
                                            const char* query,
                                            uint32_t limit,
                                            int presences,
                                            const dc_snowflake_t* user_ids,
                                            size_t user_id_count,
                                            const char* nonce) {
    if (!client || !client->gateway) return DC_ERROR_NULL_POINTER;
    return dc_gateway_client_request_guild_members(client->gateway, guild_id, query, limit,
                                                   presences, user_ids, user_id_count, nonce);
}

dc_status_t dc_client_request_soundboard_sounds(dc_client_t* client,
                                                const dc_snowflake_t* guild_ids,
                                                size_t guild_id_count) {
    if (!client || !client->gateway) return DC_ERROR_NULL_POINTER;
    return dc_gateway_client_request_soundboard_sounds(client->gateway, guild_ids, guild_id_count);
}

dc_status_t dc_client_update_voice_state(dc_client_t* client,
                                         dc_snowflake_t guild_id,
                                         dc_snowflake_t channel_id,
                                         int self_mute,
                                         int self_deaf) {
    if (!client || !client->gateway) return DC_ERROR_NULL_POINTER;
    return dc_gateway_client_update_voice_state(client->gateway, guild_id, channel_id,
                                                self_mute, self_deaf);
}
