/**
 * @file dc_commands.c
 * @brief Simple command router implementation
 */

#include "dc_commands.h"
#include "core/dc_alloc.h"
#include "json/dc_json.h"
#include <string.h>

static int dc_cmd_is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int dc_ascii_tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int dc_cmd_name_valid(const char* name) {
    if (!name || name[0] == '\0') return 0;
    for (const unsigned char* p = (const unsigned char*)name; *p != '\0'; p++) {
        if (*p <= 0x20 || *p == 0x7f) return 0;
    }
    return 1;
}

static int dc_cmd_name_eq(const char* a, const char* b, size_t len, int case_insensitive) {
    if (!a || !b) return 0;
    if (!case_insensitive) {
        return strncmp(a, b, len) == 0 && b[len] == '\0';
    }
    for (size_t i = 0; i < len; i++) {
        if (dc_ascii_tolower((unsigned char)a[i]) != dc_ascii_tolower((unsigned char)b[i])) {
            return 0;
        }
    }
    return b[len] == '\0';
}

static dc_status_t dc_cmd_message_from_json(const char* json, dc_message_t* out) {
    if (!json || !out) return DC_ERROR_NULL_POINTER;
    dc_status_t st = dc_message_from_json(json, out);
    if (st == DC_OK) return DC_OK;

    dc_message_t tmp;
    st = dc_message_init(&tmp);
    if (st != DC_OK) return st;

    dc_json_doc_t doc;
    st = dc_json_parse(json, &doc);
    if (st != DC_OK) {
        dc_message_free(&tmp);
        return st;
    }

    dc_snowflake_t channel_id = 0;
    st = dc_json_get_snowflake(doc.root, "channel_id", &channel_id);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        dc_message_free(&tmp);
        return st;
    }

    const char* content = "";
    st = dc_json_get_string_opt(doc.root, "content", &content, "");
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        dc_message_free(&tmp);
        return st;
    }
    st = dc_string_set_cstr(&tmp.content, content);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        dc_message_free(&tmp);
        return st;
    }
    tmp.channel_id = channel_id;

    const char* timestamp = "";
    st = dc_json_get_string_opt(doc.root, "timestamp", &timestamp, "");
    if (st == DC_OK && timestamp && timestamp[0] != '\0') {
        dc_string_set_cstr(&tmp.timestamp, timestamp);
    }

    yyjson_val* author = NULL;
    st = dc_json_get_object_opt(doc.root, "author", &author);
    if (st == DC_OK && author) {
        int bot = 0;
        if (dc_json_get_bool_opt(author, "bot", &bot, 0) == DC_OK) {
            tmp.author.bot = bot;
        }
        const char* username = "";
        if (dc_json_get_string_opt(author, "username", &username, "") == DC_OK) {
            dc_string_set_cstr(&tmp.author.username, username);
        }
    }

    dc_snowflake_t id = 0;
    if (dc_json_get_snowflake_opt(doc.root, "id", &id, 0) == DC_OK) {
        tmp.id = id;
    }

    dc_json_doc_free(&doc);
    *out = tmp;
    return DC_OK;
}

dc_status_t dc_command_router_init(dc_command_router_t* router, const char* prefix) {
    if (!router) return DC_ERROR_NULL_POINTER;
    memset(router, 0, sizeof(*router));

    dc_status_t st = dc_string_init(&router->prefix);
    if (st != DC_OK) return st;
    st = dc_vec_init(&router->commands, sizeof(dc_command_t));
    if (st != DC_OK) {
        dc_string_free(&router->prefix);
        return st;
    }

    router->ignore_bots = 1;
    router->case_insensitive = 1;

    const char* use_prefix = (prefix && prefix[0] != '\0') ? prefix : "!";
    st = dc_string_set_cstr(&router->prefix, use_prefix);
    if (st != DC_OK) {
        dc_vec_free(&router->commands);
        dc_string_free(&router->prefix);
        return st;
    }
    return DC_OK;
}

void dc_command_router_free(dc_command_router_t* router) {
    if (!router) return;
    dc_vec_free(&router->commands);
    dc_string_free(&router->prefix);
    memset(router, 0, sizeof(*router));
}

dc_status_t dc_command_router_add(dc_command_router_t* router, const dc_command_t* command) {
    if (!router || !command) return DC_ERROR_NULL_POINTER;
    if (!dc_cmd_name_valid(command->name)) return DC_ERROR_INVALID_PARAM;
    if (!command->handler) return DC_ERROR_INVALID_PARAM;

    size_t count = dc_vec_length(&router->commands);
    for (size_t i = 0; i < count; i++) {
        const dc_command_t* existing = (const dc_command_t*)dc_vec_at(&router->commands, i);
        if (!existing || !existing->name) continue;
        size_t name_len = strlen(command->name);
        if (dc_cmd_name_eq(command->name, existing->name, name_len, router->case_insensitive)) {
            return DC_ERROR_CONFLICT;
        }
    }
    return dc_vec_push(&router->commands, command);
}

dc_status_t dc_command_router_add_many(dc_command_router_t* router,
                                       const dc_command_t* commands,
                                       size_t count) {
    if (!router || !commands) return DC_ERROR_NULL_POINTER;
    for (size_t i = 0; i < count; i++) {
        dc_status_t st = dc_command_router_add(router, &commands[i]);
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

dc_status_t dc_command_router_set_prefix(dc_command_router_t* router, const char* prefix) {
    if (!router || !prefix || prefix[0] == '\0') return DC_ERROR_INVALID_PARAM;
    return dc_string_set_cstr(&router->prefix, prefix);
}

void dc_command_router_set_ignore_bots(dc_command_router_t* router, int ignore) {
    if (!router) return;
    router->ignore_bots = ignore ? 1 : 0;
}

void dc_command_router_set_case_insensitive(dc_command_router_t* router, int enable) {
    if (!router) return;
    router->case_insensitive = enable ? 1 : 0;
}

dc_status_t dc_command_router_handle_message(dc_command_router_t* router,
                                             dc_client_t* client,
                                             const dc_message_t* message) {
    if (!router || !message) return DC_ERROR_NULL_POINTER;
    (void)client;

    if (router->ignore_bots && message->author.bot) {
        return DC_OK;
    }

    const char* content = dc_string_cstr(&message->content);
    if (!content || content[0] == '\0') return DC_OK;

    const char* p = content;
    while (*p && dc_cmd_is_space(*p)) p++;

    const char* prefix = dc_string_cstr(&router->prefix);
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    if (prefix_len > 0) {
        if (strncmp(p, prefix, prefix_len) != 0) return DC_OK;
        p += prefix_len;
    }

    if (*p == '\0') return DC_OK;
    while (*p && dc_cmd_is_space(*p)) p++;
    if (*p == '\0') return DC_OK;

    const char* name_start = p;
    while (*p && !dc_cmd_is_space(*p)) p++;
    size_t name_len = (size_t)(p - name_start);
    if (name_len == 0) return DC_OK;

    while (*p && dc_cmd_is_space(*p)) p++;
    const char* args = p;

    size_t count = dc_vec_length(&router->commands);
    for (size_t i = 0; i < count; i++) {
        const dc_command_t* cmd = (const dc_command_t*)dc_vec_at(&router->commands, i);
        if (!cmd) continue;
        if (dc_cmd_name_eq(name_start, cmd->name, name_len, router->case_insensitive)) {
            return cmd->handler(client, message, args, router->user_data);
        }
    }
    return DC_OK;
}

dc_status_t dc_command_router_handle_event(dc_command_router_t* router,
                                           dc_client_t* client,
                                           const char* event_name,
                                           const char* event_json) {
    if (!router || !event_name || !event_json) return DC_ERROR_NULL_POINTER;
    if (strcmp(event_name, "MESSAGE_CREATE") != 0) return DC_OK;

    dc_message_t msg;
    dc_status_t st = dc_cmd_message_from_json(event_json, &msg);
    if (st != DC_OK) return st;

    st = dc_command_router_handle_message(router, client, &msg);
    dc_message_free(&msg);
    return st;
}
