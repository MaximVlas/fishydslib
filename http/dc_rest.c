/**
 * @file dc_rest.c
 * @brief REST client implementation
 */

#include "dc_rest.h"
#include "http/dc_http_compliance.h"
#include "core/dc_alloc.h"
#include "core/dc_status.h"
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

typedef struct {
    dc_string_t route_key;
    dc_string_t major;
    dc_http_rate_limit_t rl;
    uint64_t reset_at_ms;
} dc_rest_bucket_t;

typedef struct {
    dc_string_t route_key;
    dc_string_t bucket;
} dc_rest_bucket_key_t;

struct dc_rest_client {
    dc_http_client_t* http;
    dc_string_t token;
    dc_http_auth_type_t auth_type;
    dc_string_t user_agent;
    uint32_t timeout_ms;
    uint32_t max_retries;
    uint32_t global_rate_limit;
    uint32_t global_window_ms;
    uint32_t invalid_limit;
    uint32_t invalid_window_ms;
    uint64_t epoch_offset_ms;
    uint64_t global_window_start_ms;
    uint32_t global_window_count;
    uint64_t global_block_until_ms;
    uint64_t invalid_window_start_ms;
    uint32_t invalid_count;
    uint64_t invalid_block_until_ms;
    dc_vec_t buckets;      /* dc_rest_bucket_t */
    dc_vec_t bucket_keys;  /* dc_rest_bucket_key_t */
    dc_rest_transport_fn transport;
    void* transport_userdata;
    pthread_mutex_t lock;
    int lock_inited;
};

static uint64_t dc_rest_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000u);
}

static uint64_t dc_rest_epoch_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000u);
}

static void dc_rest_sleep_ms(uint64_t ms) {
    if (ms == 0) return;
    struct timespec req;
    req.tv_sec = (time_t)(ms / 1000u);
    req.tv_nsec = (long)((ms % 1000u) * 1000000u);
    while (nanosleep(&req, &req) != 0 && errno == EINTR) {
    }
}

static int dc_rest_ascii_tolower_int(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int dc_rest_ascii_strcaseeq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        int ca = dc_rest_ascii_tolower_int((unsigned char)*a);
        int cb = dc_rest_ascii_tolower_int((unsigned char)*b);
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int dc_rest_header_value_valid(const char* value) {
    if (!value) return 0;
    for (const char* p = value; *p; p++) {
        if (*p == '\r' || *p == '\n') return 0;
    }
    return 1;
}

static int dc_rest_name_value_valid(const char* name, const char* value) {
    if (!name || !value) return 0;
    if (name[0] == '\0') return 0;
    for (const char* p = name; *p; p++) {
        if (*p == '\r' || *p == '\n') return 0;
    }
    return dc_rest_header_value_valid(value);
}

static void dc_rest_header_free(dc_http_header_t* header) {
    if (!header) return;
    dc_string_free(&header->name);
    dc_string_free(&header->value);
}

static dc_status_t dc_rest_header_init(dc_http_header_t* header, const char* name, const char* value) {
    if (!header || !name || !value) return DC_ERROR_NULL_POINTER;
    dc_status_t st = dc_string_init_from_cstr(&header->name, name);
    if (st != DC_OK) return st;
    st = dc_string_init_from_cstr(&header->value, value);
    if (st != DC_OK) {
        dc_string_free(&header->name);
        return st;
    }
    return DC_OK;
}

static dc_http_header_t* dc_rest_headers_find(dc_vec_t* headers, const char* name) {
    if (!headers || !name) return NULL;
    for (size_t i = 0; i < headers->length; i++) {
        dc_http_header_t* h = (dc_http_header_t*)dc_vec_at(headers, i);
        if (h && dc_rest_ascii_strcaseeq(dc_string_cstr(&h->name), name)) {
            return h;
        }
    }
    return NULL;
}

static int dc_rest_headers_has(const dc_vec_t* headers, const char* name) {
    if (!headers || !name) return 0;
    for (size_t i = 0; i < headers->length; i++) {
        const dc_http_header_t* h = (const dc_http_header_t*)dc_vec_at((dc_vec_t*)headers, i);
        if (h && dc_rest_ascii_strcaseeq(dc_string_cstr(&h->name), name)) {
            return 1;
        }
    }
    return 0;
}

static dc_status_t dc_rest_headers_add_or_replace(dc_vec_t* headers, const char* name, const char* value) {
    if (!headers || !name || !value) return DC_ERROR_NULL_POINTER;
    dc_http_header_t* existing = dc_rest_headers_find(headers, name);
    if (existing) {
        return dc_string_set_cstr(&existing->value, value);
    }
    dc_http_header_t header;
    dc_status_t st = dc_rest_header_init(&header, name, value);
    if (st != DC_OK) return st;
    st = dc_vec_push(headers, &header);
    if (st != DC_OK) {
        dc_rest_header_free(&header);
        return st;
    }
    return DC_OK;
}

static void dc_rest_headers_clear(dc_vec_t* headers) {
    if (!headers) return;
    for (size_t i = 0; i < headers->length; i++) {
        dc_http_header_t* h = (dc_http_header_t*)dc_vec_at(headers, i);
        dc_rest_header_free(h);
    }
    dc_vec_clear(headers);
}

static int dc_rest_is_digits(const char* s, size_t len) {
    if (!s || len == 0) return 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
    }
    return 1;
}

static dc_status_t dc_rest_extract_path(const char* input, dc_string_t* out_path) {
    if (!input || !out_path) return DC_ERROR_NULL_POINTER;
    if (strncmp(input, "http://", 7) == 0) return DC_ERROR_INVALID_PARAM;
    if (strncmp(input, "https://", 8) == 0) {
        if (!dc_http_is_discord_api_url(input)) return DC_ERROR_INVALID_PARAM;
        size_t base_len = strlen(DC_DISCORD_API_BASE_URL);
        size_t in_len = strlen(input);
        if (in_len <= base_len) {
            return dc_string_set_cstr(out_path, "/");
        }
        const char* path_start = input + base_len;
        if (*path_start == '\0') {
            return dc_string_set_cstr(out_path, "/");
        }
        const char* end = path_start;
        while (*end && *end != '?' && *end != '#') end++;
        return dc_string_set_buffer(out_path, path_start, (size_t)(end - path_start));
    }
    if (input[0] == '\0') return DC_ERROR_INVALID_PARAM;
    const char* start = input;
    if (*start != '/') {
        dc_status_t st = dc_string_set_cstr(out_path, "/");
        if (st != DC_OK) return st;
        return dc_string_append_cstr(out_path, input);
    }
    const char* end = start;
    while (*end && *end != '?' && *end != '#') end++;
    return dc_string_set_buffer(out_path, start, (size_t)(end - start));
}

static int dc_rest_is_interaction_path(const char* path) {
    if (!path) return 0;
    return (strncmp(path, "/interactions/", 14) == 0);
}

static dc_status_t dc_rest_build_route_key(dc_http_method_t method, const char* path,
                                           dc_string_t* route_key, dc_string_t* major) {
    if (!path || !route_key || !major) return DC_ERROR_NULL_POINTER;
    dc_status_t st = dc_string_clear(route_key);
    if (st != DC_OK) return st;
    st = dc_string_clear(major);
    if (st != DC_OK) return st;

    const char* method_str = "GET";
    switch (method) {
        case DC_HTTP_GET: method_str = "GET"; break;
        case DC_HTTP_POST: method_str = "POST"; break;
        case DC_HTTP_PUT: method_str = "PUT"; break;
        case DC_HTTP_PATCH: method_str = "PATCH"; break;
        case DC_HTTP_DELETE: method_str = "DELETE"; break;
        case DC_HTTP_HEAD: method_str = "HEAD"; break;
        case DC_HTTP_OPTIONS: method_str = "OPTIONS"; break;
        default: method_str = "GET"; break;
    }
    st = dc_string_append_cstr(route_key, method_str);
    if (st != DC_OK) return st;
    st = dc_string_append_cstr(route_key, " ");
    if (st != DC_OK) return st;

    const char* p = path;
    const char* prev_seg = NULL;
    size_t prev_len = 0;
    int major_set = 0;
    int prev_was_webhook_id = 0;
    while (*p) {
        while (*p == '/') p++;
        if (*p == '\0') break;
        const char* seg_start = p;
        while (*p && *p != '/') p++;
        size_t seg_len = (size_t)(p - seg_start);
        int is_id = dc_rest_is_digits(seg_start, seg_len);
        const int prev_is_channels = (prev_seg && prev_len == 8 && strncmp(prev_seg, "channels", 8) == 0);
        const int prev_is_guilds = (prev_seg && prev_len == 6 && strncmp(prev_seg, "guilds", 6) == 0);
        const int prev_is_webhooks = (prev_seg && prev_len == 8 && strncmp(prev_seg, "webhooks", 8) == 0);
        const int prev_is_interactions = (prev_seg && prev_len == 12 && strncmp(prev_seg, "interactions", 12) == 0);

        st = dc_string_append_cstr(route_key, "/");
        if (st != DC_OK) return st;

        if (is_id) {
            if (!major_set && (prev_is_channels || prev_is_guilds || prev_is_webhooks || prev_is_interactions)) {
                const char* prefix = prev_is_channels ? "channels" :
                                     prev_is_guilds ? "guilds" :
                                     prev_is_webhooks ? "webhooks" :
                                     "interactions";
                st = dc_string_append_cstr(major, prefix);
                if (st != DC_OK) return st;
                st = dc_string_append_cstr(major, "/");
                if (st != DC_OK) return st;
                st = dc_string_append_buffer(major, seg_start, seg_len);
                if (st != DC_OK) return st;
                major_set = 1;
            }
            st = dc_string_append_cstr(route_key, ":id");
            if (st != DC_OK) return st;
        } else if (prev_was_webhook_id) {
            st = dc_string_append_cstr(route_key, ":token");
            if (st != DC_OK) return st;
        } else {
            st = dc_string_append_buffer(route_key, seg_start, seg_len);
            if (st != DC_OK) return st;
        }

        prev_was_webhook_id = (is_id && prev_is_webhooks) ? 1 : 0;
        prev_seg = seg_start;
        prev_len = seg_len;
    }

    if (!major_set) {
        st = dc_string_set_cstr(major, "global");
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

static dc_status_t dc_rest_bucket_init(dc_rest_bucket_t* bucket, const char* route_key,
                                       const char* major, const char* bucket_id) {
    if (!bucket || !route_key || !major) return DC_ERROR_NULL_POINTER;
    dc_status_t st = dc_string_init_from_cstr(&bucket->route_key, route_key);
    if (st != DC_OK) return st;
    st = dc_string_init_from_cstr(&bucket->major, major);
    if (st != DC_OK) {
        dc_string_free(&bucket->route_key);
        return st;
    }
    st = dc_http_rate_limit_init(&bucket->rl);
    if (st != DC_OK) {
        dc_string_free(&bucket->route_key);
        dc_string_free(&bucket->major);
        return st;
    }
    if (bucket_id && bucket_id[0] != '\0') {
        st = dc_string_set_cstr(&bucket->rl.bucket, bucket_id);
        if (st != DC_OK) {
            dc_http_rate_limit_free(&bucket->rl);
            dc_string_free(&bucket->route_key);
            dc_string_free(&bucket->major);
            return st;
        }
    }
    bucket->reset_at_ms = 0;
    return DC_OK;
}

static void dc_rest_bucket_free(dc_rest_bucket_t* bucket) {
    if (!bucket) return;
    dc_string_free(&bucket->route_key);
    dc_string_free(&bucket->major);
    dc_http_rate_limit_free(&bucket->rl);
    bucket->reset_at_ms = 0;
}

static dc_status_t dc_rest_bucket_key_init(dc_rest_bucket_key_t* key, const char* route_key,
                                           const char* bucket_id) {
    if (!key || !route_key || !bucket_id) return DC_ERROR_NULL_POINTER;
    dc_status_t st = dc_string_init_from_cstr(&key->route_key, route_key);
    if (st != DC_OK) return st;
    st = dc_string_init_from_cstr(&key->bucket, bucket_id);
    if (st != DC_OK) {
        dc_string_free(&key->route_key);
        return st;
    }
    return DC_OK;
}

static void dc_rest_bucket_key_free(dc_rest_bucket_key_t* key) {
    if (!key) return;
    dc_string_free(&key->route_key);
    dc_string_free(&key->bucket);
}

static dc_rest_bucket_t* dc_rest_find_bucket_by_id(dc_rest_client_t* client,
                                                   const char* bucket_id, const char* major) {
    if (!client || !bucket_id || !major) return NULL;
    for (size_t i = 0; i < client->buckets.length; i++) {
        dc_rest_bucket_t* bucket = (dc_rest_bucket_t*)dc_vec_at(&client->buckets, i);
        if (!bucket) continue;
        if (dc_string_compare_cstr(&bucket->rl.bucket, bucket_id) == 0 &&
            dc_string_compare_cstr(&bucket->major, major) == 0) {
            return bucket;
        }
    }
    return NULL;
}

static dc_rest_bucket_t* dc_rest_find_bucket_by_route(dc_rest_client_t* client,
                                                      const char* route_key, const char* major) {
    if (!client || !route_key || !major) return NULL;
    for (size_t i = 0; i < client->buckets.length; i++) {
        dc_rest_bucket_t* bucket = (dc_rest_bucket_t*)dc_vec_at(&client->buckets, i);
        if (!bucket) continue;
        if (dc_string_compare_cstr(&bucket->route_key, route_key) == 0 &&
            dc_string_compare_cstr(&bucket->major, major) == 0) {
            return bucket;
        }
    }
    return NULL;
}

static const char* dc_rest_find_bucket_id(dc_rest_client_t* client, const char* route_key) {
    if (!client || !route_key) return NULL;
    for (size_t i = 0; i < client->bucket_keys.length; i++) {
        dc_rest_bucket_key_t* key = (dc_rest_bucket_key_t*)dc_vec_at(&client->bucket_keys, i);
        if (!key) continue;
        if (dc_string_compare_cstr(&key->route_key, route_key) == 0) {
            return dc_string_cstr(&key->bucket);
        }
    }
    return NULL;
}

static dc_status_t dc_rest_store_bucket_id(dc_rest_client_t* client, const char* route_key,
                                           const char* bucket_id) {
    if (!client || !route_key || !bucket_id) return DC_ERROR_NULL_POINTER;
    for (size_t i = 0; i < client->bucket_keys.length; i++) {
        dc_rest_bucket_key_t* key = (dc_rest_bucket_key_t*)dc_vec_at(&client->bucket_keys, i);
        if (!key) continue;
        if (dc_string_compare_cstr(&key->route_key, route_key) == 0) {
            return dc_string_set_cstr(&key->bucket, bucket_id);
        }
    }
    dc_rest_bucket_key_t new_key;
    dc_status_t st = dc_rest_bucket_key_init(&new_key, route_key, bucket_id);
    if (st != DC_OK) return st;
    st = dc_vec_push(&client->bucket_keys, &new_key);
    if (st != DC_OK) {
        dc_rest_bucket_key_free(&new_key);
        return st;
    }
    return DC_OK;
}

static uint64_t dc_rest_epoch_to_monotonic(dc_rest_client_t* client, double epoch_seconds) {
    if (!client || epoch_seconds <= 0.0) return 0;
    uint64_t epoch_ms = (uint64_t)(epoch_seconds * 1000.0);
    if (epoch_ms < (uint64_t)client->epoch_offset_ms) return 0;
    return epoch_ms - (uint64_t)client->epoch_offset_ms;
}

static void dc_rest_update_bucket(dc_rest_client_t* client, dc_rest_bucket_t* bucket,
                                  const dc_http_rate_limit_t* rl, uint64_t now_ms) {
    if (!client || !bucket || !rl) return;
    bucket->rl.limit = rl->limit;
    bucket->rl.remaining = rl->remaining;
    bucket->rl.reset = rl->reset;
    bucket->rl.reset_after = rl->reset_after;
    bucket->rl.retry_after = rl->retry_after;
    bucket->rl.global = rl->global;
    bucket->rl.scope = rl->scope;
    if (!dc_string_is_empty(&rl->bucket)) {
        dc_string_set_cstr(&bucket->rl.bucket, dc_string_cstr(&rl->bucket));
    }
    if (rl->reset_after > 0.0) {
        bucket->reset_at_ms = now_ms + (uint64_t)(rl->reset_after * 1000.0);
    } else if (rl->reset > 0.0) {
        bucket->reset_at_ms = dc_rest_epoch_to_monotonic(client, rl->reset);
    }
}

static dc_status_t dc_rest_request_copy_headers(dc_http_request_t* http_req,
                                                const dc_rest_request_t* req) {
    if (!http_req || !req) return DC_ERROR_NULL_POINTER;
    for (size_t i = 0; i < req->headers.length; i++) {
        const dc_http_header_t* h = (const dc_http_header_t*)dc_vec_at((dc_vec_t*)&req->headers, i);
        if (!h) continue;
        dc_status_t st = dc_http_request_add_header(http_req, dc_string_cstr(&h->name), dc_string_cstr(&h->value));
        if (st != DC_OK) return st;
    }
    return DC_OK;
}

dc_status_t dc_rest_client_create(const dc_rest_client_config_t* config, dc_rest_client_t** out_client) {
    if (!config || !out_client) return DC_ERROR_NULL_POINTER;
    if (!config->token || config->token[0] == '\0') return DC_ERROR_INVALID_PARAM;
    *out_client = NULL;

    dc_rest_client_t* client = (dc_rest_client_t*)dc_alloc(sizeof(dc_rest_client_t));
    if (!client) return DC_ERROR_OUT_OF_MEMORY;
    memset(client, 0, sizeof(*client));

    dc_status_t st = dc_string_init_from_cstr(&client->token, config->token);
    if (st != DC_OK) {
        dc_free(client);
        return st;
    }

    client->auth_type = config->auth_type;
    client->timeout_ms = config->timeout_ms;
    client->max_retries = (config->max_retries == 0) ? 1u : config->max_retries;
    client->global_rate_limit = (config->global_rate_limit_per_sec == 0) ? 50u : config->global_rate_limit_per_sec;
    client->global_window_ms = (config->global_window_ms == 0) ? 1000u : config->global_window_ms;
    client->invalid_limit = (config->invalid_request_limit == 0) ? 10000u : config->invalid_request_limit;
    client->invalid_window_ms = (config->invalid_request_window_ms == 0) ? 600000u : config->invalid_request_window_ms;
    client->transport = config->transport;
    client->transport_userdata = config->transport_userdata;

    st = dc_string_init(&client->user_agent);
    if (st != DC_OK) {
        dc_string_free(&client->token);
        dc_free(client);
        return st;
    }
    if (config->user_agent && config->user_agent[0] != '\0') {
        if (!dc_http_user_agent_is_valid(config->user_agent)) {
            dc_string_free(&client->user_agent);
            dc_string_free(&client->token);
            dc_free(client);
            return DC_ERROR_INVALID_PARAM;
        }
        st = dc_string_set_cstr(&client->user_agent, config->user_agent);
        if (st != DC_OK) {
            dc_string_free(&client->user_agent);
            dc_string_free(&client->token);
            dc_free(client);
            return st;
        }
    }

    st = dc_vec_init(&client->buckets, sizeof(dc_rest_bucket_t));
    if (st != DC_OK) {
        dc_string_free(&client->user_agent);
        dc_string_free(&client->token);
        dc_free(client);
        return st;
    }
    st = dc_vec_init(&client->bucket_keys, sizeof(dc_rest_bucket_key_t));
    if (st != DC_OK) {
        dc_vec_free(&client->buckets);
        dc_string_free(&client->user_agent);
        dc_string_free(&client->token);
        dc_free(client);
        return st;
    }

    if (!client->transport) {
        st = dc_http_client_create(&client->http);
        if (st != DC_OK) {
            dc_vec_free(&client->bucket_keys);
            dc_vec_free(&client->buckets);
            dc_string_free(&client->user_agent);
            dc_string_free(&client->token);
            dc_free(client);
            return st;
        }
    }

    if (pthread_mutex_init(&client->lock, NULL) != 0) {
        if (client->http) {
            dc_http_client_free(client->http);
            client->http = NULL;
        }
        dc_vec_free(&client->bucket_keys);
        dc_vec_free(&client->buckets);
        dc_string_free(&client->user_agent);
        dc_string_free(&client->token);
        dc_free(client);
        return DC_ERROR_INVALID_STATE;
    }
    client->lock_inited = 1;

    uint64_t mono_ms = dc_rest_now_ms();
    uint64_t epoch_ms = dc_rest_epoch_ms();
    if (epoch_ms > mono_ms) {
        client->epoch_offset_ms = epoch_ms - mono_ms;
    } else {
        client->epoch_offset_ms = 0;
    }

    client->global_window_start_ms = mono_ms;
    client->invalid_window_start_ms = mono_ms;

    *out_client = client;
    return DC_OK;
}

void dc_rest_client_free(dc_rest_client_t* client) {
    if (!client) return;
    if (client->http) {
        dc_http_client_free(client->http);
    }
    if (client->lock_inited) {
        pthread_mutex_destroy(&client->lock);
        client->lock_inited = 0;
    }
    for (size_t i = 0; i < client->buckets.length; i++) {
        dc_rest_bucket_t* bucket = (dc_rest_bucket_t*)dc_vec_at(&client->buckets, i);
        dc_rest_bucket_free(bucket);
    }
    for (size_t i = 0; i < client->bucket_keys.length; i++) {
        dc_rest_bucket_key_t* key = (dc_rest_bucket_key_t*)dc_vec_at(&client->bucket_keys, i);
        dc_rest_bucket_key_free(key);
    }
    dc_vec_free(&client->buckets);
    dc_vec_free(&client->bucket_keys);
    dc_string_free(&client->user_agent);
    dc_string_free(&client->token);
    dc_free(client);
}

dc_status_t dc_rest_request_init(dc_rest_request_t* request) {
    if (!request) return DC_ERROR_NULL_POINTER;
    memset(request, 0, sizeof(*request));
    request->method = DC_HTTP_GET;
    dc_status_t st = dc_string_init(&request->path);
    if (st != DC_OK) return st;
    st = dc_vec_init(&request->headers, sizeof(dc_http_header_t));
    if (st != DC_OK) {
        dc_string_free(&request->path);
        return st;
    }
    st = dc_string_init(&request->body);
    if (st != DC_OK) {
        dc_vec_free(&request->headers);
        dc_string_free(&request->path);
        return st;
    }
    request->timeout_ms = 0;
    request->body_is_json = 0;
    request->is_interaction = 0;
    return DC_OK;
}

void dc_rest_request_free(dc_rest_request_t* request) {
    if (!request) return;
    dc_rest_headers_clear(&request->headers);
    dc_vec_free(&request->headers);
    dc_string_free(&request->path);
    dc_string_free(&request->body);
    memset(request, 0, sizeof(*request));
}

dc_status_t dc_rest_request_set_method(dc_rest_request_t* request, dc_http_method_t method) {
    if (!request) return DC_ERROR_NULL_POINTER;
    request->method = method;
    return DC_OK;
}

dc_status_t dc_rest_request_set_path(dc_rest_request_t* request, const char* path) {
    if (!request || !path) return DC_ERROR_NULL_POINTER;
    if (!dc_rest_header_value_valid(path)) return DC_ERROR_INVALID_PARAM;
    return dc_string_set_cstr(&request->path, path);
}

dc_status_t dc_rest_request_add_header(dc_rest_request_t* request, const char* name, const char* value) {
    if (!request || !name || !value) return DC_ERROR_NULL_POINTER;
    if (!dc_rest_name_value_valid(name, value)) return DC_ERROR_INVALID_PARAM;
    if (dc_rest_ascii_strcaseeq(name, "Authorization")) return DC_ERROR_INVALID_PARAM;
    if (dc_rest_ascii_strcaseeq(name, "User-Agent")) return DC_ERROR_INVALID_PARAM;
    if (dc_rest_ascii_strcaseeq(name, "Content-Type")) {
        if (!dc_http_content_type_is_allowed(value)) return DC_ERROR_INVALID_PARAM;
        if (request->body_is_json) return DC_ERROR_INVALID_PARAM;
    }
    return dc_rest_headers_add_or_replace(&request->headers, name, value);
}

dc_status_t dc_rest_request_set_body(dc_rest_request_t* request, const char* body) {
    if (!request) return DC_ERROR_NULL_POINTER;
    request->body_is_json = 0;
    if (!body) {
        return dc_string_clear(&request->body);
    }
    return dc_string_set_cstr(&request->body, body);
}

dc_status_t dc_rest_request_set_body_buffer(dc_rest_request_t* request, const void* body, size_t length) {
    if (!request) return DC_ERROR_NULL_POINTER;
    request->body_is_json = 0;
    if (!body) {
        return (length == 0) ? dc_string_clear(&request->body) : DC_ERROR_NULL_POINTER;
    }
    if (length == 0) return dc_string_clear(&request->body);
    return dc_string_set_buffer(&request->body, (const char*)body, length);
}

dc_status_t dc_rest_request_set_json_body(dc_rest_request_t* request, const char* json_body) {
    if (!request || !json_body) return DC_ERROR_NULL_POINTER;
    dc_status_t st = dc_http_validate_json_body(json_body, 0);
    if (st != DC_OK) return st;
    st = dc_string_set_cstr(&request->body, json_body);
    if (st != DC_OK) return st;
    request->body_is_json = 1;
    return dc_rest_headers_add_or_replace(&request->headers, "Content-Type", "application/json");
}

dc_status_t dc_rest_request_set_timeout(dc_rest_request_t* request, uint32_t timeout_ms) {
    if (!request) return DC_ERROR_NULL_POINTER;
    request->timeout_ms = timeout_ms;
    return DC_OK;
}

dc_status_t dc_rest_request_set_interaction(dc_rest_request_t* request, int is_interaction) {
    if (!request) return DC_ERROR_NULL_POINTER;
    request->is_interaction = is_interaction ? 1 : 0;
    return DC_OK;
}

dc_status_t dc_rest_response_init(dc_rest_response_t* response) {
    if (!response) return DC_ERROR_NULL_POINTER;
    memset(response, 0, sizeof(*response));
    dc_status_t st = dc_http_response_init(&response->http);
    if (st != DC_OK) return st;
    st = dc_http_error_init(&response->error);
    if (st != DC_OK) {
        dc_http_response_free(&response->http);
        return st;
    }
    st = dc_http_rate_limit_init(&response->rate_limit);
    if (st != DC_OK) {
        dc_http_error_free(&response->error);
        dc_http_response_free(&response->http);
        return st;
    }
    st = dc_http_rate_limit_response_init(&response->rate_limit_response);
    if (st != DC_OK) {
        dc_http_rate_limit_free(&response->rate_limit);
        dc_http_error_free(&response->error);
        dc_http_response_free(&response->http);
        return st;
    }
    return DC_OK;
}

void dc_rest_response_free(dc_rest_response_t* response) {
    if (!response) return;
    dc_http_rate_limit_response_free(&response->rate_limit_response);
    dc_http_rate_limit_free(&response->rate_limit);
    dc_http_error_free(&response->error);
    dc_http_response_free(&response->http);
    memset(response, 0, sizeof(*response));
}

static dc_status_t dc_rest_response_reset(dc_rest_response_t* response) {
    if (!response) return DC_ERROR_NULL_POINTER;
    dc_http_rate_limit_response_free(&response->rate_limit_response);
    dc_http_rate_limit_free(&response->rate_limit);
    dc_http_error_free(&response->error);
    dc_http_response_free(&response->http);
    dc_status_t st = dc_rest_response_init(response);
    return st;
}

static void dc_rest_update_global_limit(dc_rest_client_t* client, const dc_http_rate_limit_t* rl,
                                        const dc_http_rate_limit_response_t* body_rl, uint64_t now_ms) {
    if (!client) return;
    double retry_after = 0.0;
    if (rl && rl->retry_after > 0.0) retry_after = rl->retry_after;
    if (body_rl && body_rl->retry_after > 0.0) retry_after = body_rl->retry_after;
    if (retry_after <= 0.0) return;
    uint64_t wait_ms = (uint64_t)(retry_after * 1000.0);
    client->global_block_until_ms = now_ms + wait_ms;
}

static void dc_rest_handle_invalid_request(dc_rest_client_t* client, uint64_t now_ms) {
    if (!client) return;
    if (now_ms - client->invalid_window_start_ms >= (uint64_t)client->invalid_window_ms) {
        client->invalid_window_start_ms = now_ms;
        client->invalid_count = 0;
    }
    client->invalid_count++;
    if (client->invalid_count >= client->invalid_limit) {
        client->invalid_block_until_ms = client->invalid_window_start_ms + client->invalid_window_ms;
    }
}

dc_status_t dc_rest_execute(dc_rest_client_t* client, const dc_rest_request_t* request,
                            dc_rest_response_t* response) {
    if (!client || !request || !response) return DC_ERROR_NULL_POINTER;
    if (dc_string_is_empty(&request->path)) return DC_ERROR_INVALID_PARAM;

    uint32_t attempts = 0;
    dc_status_t st = DC_OK;

    while (attempts <= client->max_retries) {
        attempts++;
        int retry_request = 0;

        dc_string_t path;
        dc_string_t route_key;
        dc_string_t major;
        dc_http_request_t http_req;
        dc_http_rate_limit_t parsed_rl;
        dc_http_rate_limit_response_t parsed_body_rl;
        int path_inited = 0;
        int route_key_inited = 0;
        int major_inited = 0;
        int http_req_inited = 0;
        int parsed_rl_inited = 0;
        int parsed_body_rl_inited = 0;
        uint64_t now_ms = dc_rest_now_ms();

        st = dc_string_init(&path);
        if (st != DC_OK) goto cleanup_iteration;
        path_inited = 1;
        st = dc_string_init(&route_key);
        if (st != DC_OK) {
            goto cleanup_iteration;
        }
        route_key_inited = 1;
        st = dc_string_init(&major);
        if (st != DC_OK) {
            goto cleanup_iteration;
        }
        major_inited = 1;

        st = dc_rest_extract_path(dc_string_cstr(&request->path), &path);
        if (st != DC_OK) goto cleanup_iteration;

        int is_interaction = request->is_interaction ? 1 : dc_rest_is_interaction_path(dc_string_cstr(&path));
        st = dc_rest_build_route_key(request->method, dc_string_cstr(&path), &route_key, &major);
        if (st != DC_OK) goto cleanup_iteration;

        for (;;) {
            uint64_t sleep_ms = 0;
            const char* mapped_bucket_id = NULL;
            dc_rest_bucket_t* bucket = NULL;

            if (client->lock_inited && pthread_mutex_lock(&client->lock) != 0) {
                st = DC_ERROR_INVALID_STATE;
                goto cleanup_iteration;
            }

            now_ms = dc_rest_now_ms();
            if (client->invalid_block_until_ms > now_ms) {
                if (client->lock_inited) pthread_mutex_unlock(&client->lock);
                st = DC_ERROR_INVALID_STATE;
                goto cleanup_iteration;
            }

            if (!is_interaction) {
                if (client->global_block_until_ms > now_ms) {
                    sleep_ms = client->global_block_until_ms - now_ms;
                } else {
                    if (now_ms - client->global_window_start_ms >= client->global_window_ms) {
                        client->global_window_start_ms = now_ms;
                        client->global_window_count = 0;
                    }
                    if (client->global_window_count >= client->global_rate_limit) {
                        sleep_ms = client->global_window_ms - (now_ms - client->global_window_start_ms);
                    }
                }
            }

            if (sleep_ms == 0) {
                mapped_bucket_id = dc_rest_find_bucket_id(client, dc_string_cstr(&route_key));
                if (mapped_bucket_id && mapped_bucket_id[0] != '\0') {
                    bucket = dc_rest_find_bucket_by_id(client, mapped_bucket_id, dc_string_cstr(&major));
                }
                if (!bucket) {
                    bucket = dc_rest_find_bucket_by_route(client, dc_string_cstr(&route_key), dc_string_cstr(&major));
                }
                if (!bucket) {
                    dc_rest_bucket_t new_bucket;
                    st = dc_rest_bucket_init(&new_bucket, dc_string_cstr(&route_key),
                                             dc_string_cstr(&major),
                                             mapped_bucket_id ? mapped_bucket_id : "");
                    if (st != DC_OK) {
                        if (client->lock_inited) pthread_mutex_unlock(&client->lock);
                        goto cleanup_iteration;
                    }
                    st = dc_vec_push(&client->buckets, &new_bucket);
                    if (st != DC_OK) {
                        dc_rest_bucket_free(&new_bucket);
                        if (client->lock_inited) pthread_mutex_unlock(&client->lock);
                        goto cleanup_iteration;
                    }
                    bucket = (dc_rest_bucket_t*)dc_vec_at(&client->buckets, client->buckets.length - 1);
                }

                if (bucket && bucket->rl.remaining == 0 && bucket->reset_at_ms > now_ms) {
                    sleep_ms = bucket->reset_at_ms - now_ms;
                }
            }

            if (client->lock_inited) pthread_mutex_unlock(&client->lock);

            if (sleep_ms == 0) break;
            dc_rest_sleep_ms(sleep_ms);
        }

        st = dc_http_request_init(&http_req);
        if (st != DC_OK) goto cleanup_iteration;
        http_req_inited = 1;
        st = dc_http_request_set_method(&http_req, request->method);
        if (st != DC_OK) goto cleanup_iteration;
        st = dc_http_request_set_url(&http_req, dc_string_cstr(&request->path));
        if (st != DC_OK) goto cleanup_iteration;

        if (request->timeout_ms > 0) {
            dc_http_request_set_timeout(&http_req, request->timeout_ms);
        } else if (client->timeout_ms > 0) {
            dc_http_request_set_timeout(&http_req, client->timeout_ms);
        }

        if (request->body_is_json) {
            st = dc_http_request_set_json_body(&http_req, dc_string_cstr(&request->body));
            if (st != DC_OK) goto cleanup_iteration;
        } else if (!dc_string_is_empty(&request->body)) {
            st = dc_http_request_set_body(&http_req, dc_string_cstr(&request->body));
            if (st != DC_OK) goto cleanup_iteration;
        }

        if (!dc_string_is_empty(&client->user_agent)) {
            st = dc_http_request_add_header(&http_req, "User-Agent", dc_string_cstr(&client->user_agent));
            if (st != DC_OK) goto cleanup_iteration;
        }

        dc_string_t auth;
        if (dc_string_init(&auth) != DC_OK) {
            st = DC_ERROR_OUT_OF_MEMORY;
            goto cleanup_iteration;
        }
        st = dc_http_format_auth_header(client->auth_type, dc_string_cstr(&client->token), &auth);
        if (st != DC_OK) {
            dc_string_free(&auth);
            goto cleanup_iteration;
        }
        st = dc_http_request_add_header(&http_req, "Authorization", dc_string_cstr(&auth));
        dc_string_free(&auth);
        if (st != DC_OK) goto cleanup_iteration;

        st = dc_rest_request_copy_headers(&http_req, request);
        if (st != DC_OK) goto cleanup_iteration;

        if (!request->body_is_json && !dc_string_is_empty(&request->body)) {
            if (!dc_rest_headers_has(&request->headers, "Content-Type")) {
                st = DC_ERROR_INVALID_PARAM;
                goto cleanup_iteration;
            }
        }

        st = dc_rest_response_reset(response);
        if (st != DC_OK) goto cleanup_iteration;

        if (!client->transport) {
            st = dc_http_client_execute(client->http, &http_req, &response->http);
        } else {
            st = client->transport(client->transport_userdata, &http_req, &response->http);
        }
        if (st != DC_OK) goto cleanup_iteration;

        st = dc_http_rate_limit_init(&parsed_rl);
        if (st != DC_OK) goto cleanup_iteration;
        parsed_rl_inited = 1;
        dc_http_response_parse_rate_limit(&response->http, &parsed_rl);
        st = dc_http_rate_limit_response_init(&parsed_body_rl);
        if (st != DC_OK) {
            goto cleanup_iteration;
        }
        parsed_body_rl_inited = 1;

        if (response->http.status_code == 429) {
            dc_http_rate_limit_response_parse(dc_string_cstr(&response->http.body),
                                              dc_string_length(&response->http.body),
                                              &parsed_body_rl);
        }

        response->rate_limit.limit = parsed_rl.limit;
        response->rate_limit.remaining = parsed_rl.remaining;
        response->rate_limit.reset = parsed_rl.reset;
        response->rate_limit.reset_after = parsed_rl.reset_after;
        response->rate_limit.retry_after = parsed_rl.retry_after;
        response->rate_limit.global = parsed_rl.global;
        response->rate_limit.scope = parsed_rl.scope;
        dc_string_set_cstr(&response->rate_limit.bucket, dc_string_cstr(&parsed_rl.bucket));

        response->rate_limit_response.retry_after = parsed_body_rl.retry_after;
        response->rate_limit_response.global = parsed_body_rl.global;
        response->rate_limit_response.code = parsed_body_rl.code;
        dc_string_set_cstr(&response->rate_limit_response.message,
                           dc_string_cstr(&parsed_body_rl.message));

        if (response->http.status_code >= 400) {
            dc_http_error_parse(dc_string_cstr(&response->http.body),
                                dc_string_length(&response->http.body),
                                &response->error);
        }

        if (client->lock_inited && pthread_mutex_lock(&client->lock) != 0) {
            st = DC_ERROR_INVALID_STATE;
            goto cleanup_iteration;
        }

        now_ms = dc_rest_now_ms();
        if (!is_interaction) {
            if (now_ms - client->global_window_start_ms >= client->global_window_ms) {
                client->global_window_start_ms = now_ms;
                client->global_window_count = 0;
            }
            client->global_window_count++;
        }

        const char* mapped_bucket_id = dc_rest_find_bucket_id(client, dc_string_cstr(&route_key));
        dc_rest_bucket_t* bucket = NULL;
        if (mapped_bucket_id && mapped_bucket_id[0] != '\0') {
            bucket = dc_rest_find_bucket_by_id(client, mapped_bucket_id, dc_string_cstr(&major));
        }
        if (!bucket) {
            bucket = dc_rest_find_bucket_by_route(client, dc_string_cstr(&route_key), dc_string_cstr(&major));
        }
        if (!bucket) {
            dc_rest_bucket_t new_bucket;
            st = dc_rest_bucket_init(&new_bucket, dc_string_cstr(&route_key),
                                     dc_string_cstr(&major),
                                     mapped_bucket_id ? mapped_bucket_id : "");
            if (st != DC_OK) {
                if (client->lock_inited) pthread_mutex_unlock(&client->lock);
                goto cleanup_iteration;
            }
            st = dc_vec_push(&client->buckets, &new_bucket);
            if (st != DC_OK) {
                dc_rest_bucket_free(&new_bucket);
                if (client->lock_inited) pthread_mutex_unlock(&client->lock);
                goto cleanup_iteration;
            }
            bucket = (dc_rest_bucket_t*)dc_vec_at(&client->buckets, client->buckets.length - 1);
        }

        if (bucket) {
            dc_rest_update_bucket(client, bucket, &parsed_rl, now_ms);
            if (!dc_string_is_empty(&parsed_rl.bucket)) {
                dc_rest_store_bucket_id(client, dc_string_cstr(&route_key),
                                        dc_string_cstr(&parsed_rl.bucket));
            }
        }

        if (response->http.status_code == 401 ||
            response->http.status_code == 403 ||
            response->http.status_code == 429) {
            dc_rest_handle_invalid_request(client, now_ms);
        }

        if (response->http.status_code == 429 &&
            (parsed_rl.global || parsed_body_rl.global)) {
            dc_rest_update_global_limit(client, &parsed_rl, &parsed_body_rl, now_ms);
        }

        if (client->lock_inited) pthread_mutex_unlock(&client->lock);

        if (response->http.status_code == 429) {
            double retry_after = parsed_rl.retry_after > 0.0 ? parsed_rl.retry_after : parsed_body_rl.retry_after;
            if (retry_after > 0.0 && attempts <= client->max_retries) {
                dc_rest_sleep_ms((uint64_t)(retry_after * 1000.0));
                retry_request = 1;
                st = DC_OK;
                goto cleanup_iteration;
            }
        }

        st = dc_status_from_http(response->http.status_code);
        goto cleanup_iteration;

        cleanup_iteration:
        if (parsed_body_rl_inited) {
            dc_http_rate_limit_response_free(&parsed_body_rl);
        }
        if (parsed_rl_inited) {
            dc_http_rate_limit_free(&parsed_rl);
        }
        if (http_req_inited) {
            dc_http_request_free(&http_req);
        }
        if (major_inited) {
            dc_string_free(&major);
        }
        if (route_key_inited) {
            dc_string_free(&route_key);
        }
        if (path_inited) {
            dc_string_free(&path);
        }
        if (retry_request) {
            continue;
        }
        return st;
    }

    return DC_ERROR_TRY_AGAIN;
}
