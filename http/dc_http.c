/**
 * @file dc_http.c
 * @brief HTTP client implementation using libcurl (compliance-focused)
 */

#include "dc_http.h"
#include "http/dc_http_compliance.h"
#include "core/dc_alloc.h"
#include <curl/curl.h>
#include <string.h>
#include <ctype.h>

struct dc_http_client {
    CURL* curl;
};

static int g_curl_refcount = 0;

static dc_status_t dc_curl_global_acquire(void) {
    if (g_curl_refcount == 0) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
            return DC_ERROR_NETWORK;
        }
    }
    g_curl_refcount++;
    return DC_OK;
}

static void dc_curl_global_release(void) {
    if (g_curl_refcount > 0) {
        g_curl_refcount--;
        if (g_curl_refcount == 0) {
            curl_global_cleanup();
        }
    }
}

static int dc_ascii_tolower_int(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int dc_ascii_strcaseeq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        int ca = dc_ascii_tolower_int((unsigned char)*a);
        int cb = dc_ascii_tolower_int((unsigned char)*b);
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int dc_http_header_value_valid(const char* value) {
    if (!value) return 0;
    for (const char* p = value; *p; p++) {
        if (*p == '\r' || *p == '\n') return 0;
    }
    return 1;
}

static int dc_http_header_name_valid(const char* name) {
    if (!name || name[0] == '\0') return 0;
    for (const char* p = name; *p; p++) {
        if (*p == '\r' || *p == '\n') return 0;
    }
    return 1;
}

static void dc_http_header_free(dc_http_header_t* header) {
    if (!header) return;
    dc_string_free(&header->name);
    dc_string_free(&header->value);
}

static dc_status_t dc_http_header_init(dc_http_header_t* header, const char* name, const char* value) {
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

static dc_http_header_t* dc_http_headers_find(dc_vec_t* headers, const char* name) {
    if (!headers || !name) return NULL;
    for (size_t i = 0; i < headers->length; i++) {
        dc_http_header_t* h = (dc_http_header_t*)dc_vec_at(headers, i);
        if (h && dc_ascii_strcaseeq(dc_string_cstr(&h->name), name)) {
            return h;
        }
    }
    return NULL;
}

static const char* dc_http_headers_get_value(const dc_vec_t* headers, const char* name) {
    if (!headers || !name) return NULL;
    for (size_t i = 0; i < headers->length; i++) {
        dc_http_header_t* h = (dc_http_header_t*)dc_vec_at((dc_vec_t*)headers, i);
        if (h && dc_ascii_strcaseeq(dc_string_cstr(&h->name), name)) {
            return dc_string_cstr(&h->value);
        }
    }
    return NULL;
}

static dc_status_t dc_http_headers_add_or_replace(dc_vec_t* headers, const char* name, const char* value) {
    if (!headers || !name || !value) return DC_ERROR_NULL_POINTER;
    dc_http_header_t* existing = dc_http_headers_find(headers, name);
    if (existing) {
        return dc_string_set_cstr(&existing->value, value);
    }
    dc_http_header_t header;
    dc_status_t st = dc_http_header_init(&header, name, value);
    if (st != DC_OK) return st;
    st = dc_vec_push(headers, &header);
    if (st != DC_OK) {
        dc_http_header_free(&header);
        return st;
    }
    /* Ownership moved into vector */
    return DC_OK;
}

static void dc_http_headers_clear(dc_vec_t* headers) {
    if (!headers) return;
    for (size_t i = 0; i < headers->length; i++) {
        dc_http_header_t* h = (dc_http_header_t*)dc_vec_at(headers, i);
        dc_http_header_free(h);
    }
    dc_vec_clear(headers);
}

dc_status_t dc_http_client_create(dc_http_client_t** client) {
    if (!client) return DC_ERROR_NULL_POINTER;
    *client = NULL;

    dc_status_t st = dc_curl_global_acquire();
    if (st != DC_OK) return st;

    dc_http_client_t* c = (dc_http_client_t*)dc_alloc(sizeof(dc_http_client_t));
    if (!c) {
        dc_curl_global_release();
        return DC_ERROR_OUT_OF_MEMORY;
    }
    c->curl = curl_easy_init();
    if (!c->curl) {
        dc_free(c);
        dc_curl_global_release();
        return DC_ERROR_NETWORK;
    }

    *client = c;
    return DC_OK;
}

void dc_http_client_free(dc_http_client_t* client) {
    if (!client) return;
    if (client->curl) {
        curl_easy_cleanup(client->curl);
        client->curl = NULL;
    }
    dc_free(client);
    dc_curl_global_release();
}

dc_status_t dc_http_request_init(dc_http_request_t* request) {
    if (!request) return DC_ERROR_NULL_POINTER;
    request->method = DC_HTTP_GET;
    request->timeout_ms = 0;
    dc_status_t st = dc_string_init(&request->url);
    if (st != DC_OK) return st;
    st = dc_vec_init(&request->headers, sizeof(dc_http_header_t));
    if (st != DC_OK) {
        dc_string_free(&request->url);
        return st;
    }
    st = dc_string_init(&request->body);
    if (st != DC_OK) {
        dc_vec_free(&request->headers);
        dc_string_free(&request->url);
        return st;
    }
    return DC_OK;
}

void dc_http_request_free(dc_http_request_t* request) {
    if (!request) return;
    dc_http_headers_clear(&request->headers);
    dc_vec_free(&request->headers);
    dc_string_free(&request->url);
    dc_string_free(&request->body);
    request->method = DC_HTTP_GET;
    request->timeout_ms = 0;
}

dc_status_t dc_http_response_init(dc_http_response_t* response) {
    if (!response) return DC_ERROR_NULL_POINTER;
    response->status_code = 0;
    response->total_time = 0.0;
    dc_status_t st = dc_vec_init(&response->headers, sizeof(dc_http_header_t));
    if (st != DC_OK) return st;
    st = dc_string_init(&response->body);
    if (st != DC_OK) {
        dc_vec_free(&response->headers);
        return st;
    }
    return DC_OK;
}

void dc_http_response_free(dc_http_response_t* response) {
    if (!response) return;
    dc_http_headers_clear(&response->headers);
    dc_vec_free(&response->headers);
    dc_string_free(&response->body);
    response->status_code = 0;
    response->total_time = 0.0;
}

dc_status_t dc_http_request_set_method(dc_http_request_t* request, dc_http_method_t method) {
    if (!request) return DC_ERROR_NULL_POINTER;
    request->method = method;
    return DC_OK;
}

dc_status_t dc_http_request_set_url(dc_http_request_t* request, const char* url) {
    if (!request || !url) return DC_ERROR_NULL_POINTER;
    if (!dc_http_header_value_valid(url)) return DC_ERROR_INVALID_PARAM;
    return dc_http_build_discord_api_url(url, &request->url);
}

dc_status_t dc_http_request_add_header(dc_http_request_t* request, const char* name, const char* value) {
    if (!request || !name || !value) return DC_ERROR_NULL_POINTER;
    if (!dc_http_header_name_valid(name) || !dc_http_header_value_valid(value)) {
        return DC_ERROR_INVALID_PARAM;
    }

    if (dc_ascii_strcaseeq(name, "Content-Type")) {
        if (!dc_http_content_type_is_allowed(value)) {
            return DC_ERROR_INVALID_PARAM;
        }
    }
    if (dc_ascii_strcaseeq(name, "User-Agent")) {
        if (!dc_http_user_agent_is_valid(value)) {
            return DC_ERROR_INVALID_PARAM;
        }
    }
    return dc_http_headers_add_or_replace(&request->headers, name, value);
}

dc_status_t dc_http_request_set_body(dc_http_request_t* request, const char* body) {
    if (!request) return DC_ERROR_NULL_POINTER;
    if (!body) {
        return dc_string_clear(&request->body);
    }
    return dc_string_set_cstr(&request->body, body);
}

dc_status_t dc_http_request_set_body_buffer(dc_http_request_t* request, const void* body, size_t length) {
    if (!request) return DC_ERROR_NULL_POINTER;
    if (!body) {
        return (length == 0) ? dc_string_clear(&request->body) : DC_ERROR_NULL_POINTER;
    }
    if (length == 0) return dc_string_clear(&request->body);
    return dc_string_set_buffer(&request->body, (const char*)body, length);
}

dc_status_t dc_http_request_set_json_body(dc_http_request_t* request, const char* json_body) {
    if (!request || !json_body) return DC_ERROR_NULL_POINTER;
    dc_status_t st = dc_http_validate_json_body(json_body, 0);
    if (st != DC_OK) return st;
    st = dc_http_request_add_header(request, "Content-Type", "application/json");
    if (st != DC_OK) return st;
    return dc_http_request_set_body(request, json_body);
}

dc_status_t dc_http_request_set_timeout(dc_http_request_t* request, uint32_t timeout_ms) {
    if (!request) return DC_ERROR_NULL_POINTER;
    request->timeout_ms = timeout_ms;
    return DC_OK;
}

static size_t dc_http_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    if (!userdata || total == 0) return 0;
    dc_http_response_t* response = (dc_http_response_t*)userdata;
    if (dc_string_append_buffer(&response->body, ptr, total) != DC_OK) {
        return 0;
    }
    return total;
}

static size_t dc_http_header_cb(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total = size * nitems;
    if (!userdata || total == 0) return 0;

    dc_http_response_t* response = (dc_http_response_t*)userdata;
    if (total < 2) return total;

    /* Ignore status line and empty lines */
    if ((total >= 5) && (memcmp(buffer, "HTTP/", 5) == 0)) return total;
    if (buffer[0] == '\r' || buffer[0] == '\n') return total;

    char* colon = memchr(buffer, ':', total);
    if (!colon) return total;

    size_t name_len = (size_t)(colon - buffer);
    size_t value_len = total - name_len - 1;
    char* value_start = colon + 1;

    while (value_len > 0 && (*value_start == ' ' || *value_start == '\t')) {
        value_start++;
        value_len--;
    }
    while (value_len > 0 && (value_start[value_len - 1] == '\r' || value_start[value_len - 1] == '\n')) {
        value_len--;
    }

    char name_buf[256];
    char* name_ptr = name_buf;
    if (name_len == 0) return total;
    if (name_len >= sizeof(name_buf)) {
        name_ptr = (char*)dc_alloc(name_len + 1);
        if (!name_ptr) return 0;
    }
    memcpy(name_ptr, buffer, name_len);
    name_ptr[name_len] = '\0';

    dc_string_t value_str;
    if (dc_string_init_from_buffer(&value_str, value_start, value_len) != DC_OK) {
        if (name_ptr != name_buf) dc_free(name_ptr);
        return 0;
    }
    dc_http_header_t header;
    if (dc_http_header_init(&header, name_ptr, dc_string_cstr(&value_str)) != DC_OK) {
        dc_string_free(&value_str);
        if (name_ptr != name_buf) dc_free(name_ptr);
        return 0;
    }
    dc_string_free(&value_str);
    if (name_ptr != name_buf) dc_free(name_ptr);
    if (dc_vec_push(&response->headers, &header) != DC_OK) {
        dc_http_header_free(&header);
        return 0;
    }
    return total;
}

dc_status_t dc_http_client_execute(dc_http_client_t* client,
                                   const dc_http_request_t* request,
                                   dc_http_response_t* response) {
    if (!client || !request || !response) return DC_ERROR_NULL_POINTER;
    if (!request->url.data || request->url.length == 0) return DC_ERROR_INVALID_PARAM;
    if (!dc_http_is_discord_api_url(dc_string_cstr(&request->url))) {
        return DC_ERROR_INVALID_PARAM;
    }
    if (response->headers.element_size != sizeof(dc_http_header_t)) {
        return DC_ERROR_INVALID_PARAM;
    }

    const char* content_type = dc_http_headers_get_value(&request->headers, "Content-Type");
    if (request->body.length > 0) {
        if (!content_type || !dc_http_content_type_is_allowed(content_type)) {
            return DC_ERROR_INVALID_PARAM;
        }
    }

    dc_http_headers_clear(&response->headers);
    dc_string_clear(&response->body);
    response->status_code = 0;
    response->total_time = 0.0;

    curl_easy_reset(client->curl);
    curl_easy_setopt(client->curl, CURLOPT_URL, dc_string_cstr(&request->url));
    curl_easy_setopt(client->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, dc_http_write_cb);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(client->curl, CURLOPT_HEADERFUNCTION, dc_http_header_cb);
    curl_easy_setopt(client->curl, CURLOPT_HEADERDATA, response);

    if (request->timeout_ms > 0) {
        curl_easy_setopt(client->curl, CURLOPT_TIMEOUT_MS, (long)request->timeout_ms);
        curl_easy_setopt(client->curl, CURLOPT_CONNECTTIMEOUT_MS, (long)request->timeout_ms);
    }

    switch (request->method) {
        case DC_HTTP_GET:
            curl_easy_setopt(client->curl, CURLOPT_HTTPGET, 1L);
            break;
        case DC_HTTP_POST:
            curl_easy_setopt(client->curl, CURLOPT_POST, 1L);
            break;
        case DC_HTTP_PUT:
            curl_easy_setopt(client->curl, CURLOPT_CUSTOMREQUEST, "PUT");
            break;
        case DC_HTTP_PATCH:
            curl_easy_setopt(client->curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            break;
        case DC_HTTP_DELETE:
            curl_easy_setopt(client->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
        case DC_HTTP_HEAD:
            curl_easy_setopt(client->curl, CURLOPT_NOBODY, 1L);
            break;
        case DC_HTTP_OPTIONS:
            curl_easy_setopt(client->curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
            break;
        default:
            return DC_ERROR_INVALID_PARAM;
    }

    if (request->body.length > 0) {
        curl_easy_setopt(client->curl, CURLOPT_POSTFIELDS, request->body.data);
        curl_easy_setopt(client->curl, CURLOPT_POSTFIELDSIZE, (long)request->body.length);
    }

    struct curl_slist* header_list = NULL;
    for (size_t i = 0; i < request->headers.length; i++) {
        dc_http_header_t* h = (dc_http_header_t*)dc_vec_at((dc_vec_t*)&request->headers, i);
        if (!h) continue;
        dc_string_t line;
        if (dc_string_init(&line) != DC_OK) {
            curl_slist_free_all(header_list);
            return DC_ERROR_OUT_OF_MEMORY;
        }
        dc_status_t st = dc_string_printf(&line, "%s: %s", dc_string_cstr(&h->name), dc_string_cstr(&h->value));
        if (st != DC_OK) {
            dc_string_free(&line);
            curl_slist_free_all(header_list);
            return st;
        }
        struct curl_slist* new_list = curl_slist_append(header_list, dc_string_cstr(&line));
        dc_string_free(&line);
        if (!new_list) {
            curl_slist_free_all(header_list);
            return DC_ERROR_OUT_OF_MEMORY;
        }
        header_list = new_list;
    }

    dc_string_t ua;
    int has_ua = (dc_http_headers_get_value(&request->headers, "User-Agent") != NULL);
    if (!has_ua) {
        if (dc_string_init(&ua) != DC_OK) {
            curl_slist_free_all(header_list);
            return DC_ERROR_OUT_OF_MEMORY;
        }
        if (dc_http_format_default_user_agent(&ua) != DC_OK) {
            dc_string_free(&ua);
            curl_slist_free_all(header_list);
            return DC_ERROR_INVALID_PARAM;
        }
        curl_easy_setopt(client->curl, CURLOPT_USERAGENT, dc_string_cstr(&ua));
    }

    if (header_list) {
        curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, header_list);
    }

    CURLcode res = curl_easy_perform(client->curl);
    if (header_list) curl_slist_free_all(header_list);
    if (!has_ua) dc_string_free(&ua);

    if (res != CURLE_OK) {
        return DC_ERROR_NETWORK;
    }

    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &response->status_code);
    curl_easy_getinfo(client->curl, CURLINFO_TOTAL_TIME, &response->total_time);

    return DC_OK;
}

dc_status_t dc_http_response_get_header(const dc_http_response_t* response,
                                        const char* name, const char** value) {
    if (!response || !name || !value) return DC_ERROR_NULL_POINTER;
    *value = NULL;
    for (size_t i = 0; i < response->headers.length; i++) {
        dc_http_header_t* h = (dc_http_header_t*)dc_vec_at((dc_vec_t*)&response->headers, i);
        if (h && dc_ascii_strcaseeq(dc_string_cstr(&h->name), name)) {
            *value = dc_string_cstr(&h->value);
            return DC_OK;
        }
    }
    return DC_ERROR_NOT_FOUND;
}

static dc_status_t dc_http_response_get_header_cb(void* userdata, const char* name, const char** value) {
    return dc_http_response_get_header((const dc_http_response_t*)userdata, name, value);
}

dc_status_t dc_http_response_parse_rate_limit(const dc_http_response_t* response,
                                               dc_http_rate_limit_t* rl) {
    if (!response || !rl) return DC_ERROR_NULL_POINTER;
    return dc_http_rate_limit_parse(dc_http_response_get_header_cb, (void*)response, rl);
}
