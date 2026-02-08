/**
 * @file dc_multipart.c
 * @brief Multipart/form-data builder
 */

#include "dc_multipart.h"
#include "core/dc_attachments.h"
#include <stdio.h>
#include <string.h>

static unsigned long g_multipart_counter = 0;

static int dc_multipart_value_is_valid(const char* value) {
    if (!value) return 0;
    for (const char* p = value; *p; p++) {
        if (*p == '\r' || *p == '\n') return 0;
    }
    return 1;
}

static dc_status_t dc_multipart_set_default_boundary(dc_multipart_t* mp) {
    g_multipart_counter++;
    return dc_string_printf(&mp->boundary, "dc_boundary_%lu", g_multipart_counter);
}

static dc_status_t dc_multipart_append_boundary(dc_multipart_t* mp) {
    if (!mp) return DC_ERROR_NULL_POINTER;
    if (dc_string_is_empty(&mp->boundary)) return DC_ERROR_INVALID_PARAM;
    dc_status_t st = dc_string_append_cstr(&mp->body, "--");
    if (st != DC_OK) return st;
    st = dc_string_append_string(&mp->body, &mp->boundary);
    if (st != DC_OK) return st;
    return dc_string_append_cstr(&mp->body, "\r\n");
}

static size_t dc_multipart_estimate_simple_part(const char* boundary,
                                                const char* name,
                                                const char* value,
                                                const char* content_type) {
    size_t size = 0;
    size_t boundary_len = strlen(boundary);
    size += 2 + boundary_len + 2; /* --boundary\r\n */
    size += strlen("Content-Disposition: form-data; name=\"\"\r\n") + strlen(name);
    if (content_type) {
        size += strlen("Content-Type: \r\n") + strlen(content_type);
    }
    size += 2; /* \r\n */
    size += strlen(value);
    size += 2; /* \r\n */
    return size;
}

static size_t dc_multipart_estimate_file_part(const char* boundary,
                                              const char* field_name,
                                              const char* filename,
                                              size_t data_size,
                                              const char* content_type) {
    size_t size = 0;
    size_t boundary_len = strlen(boundary);
    size += 2 + boundary_len + 2; /* --boundary\r\n */
    size += strlen("Content-Disposition: form-data; name=\"\"; filename=\"\"\r\n") +
            strlen(field_name) + strlen(filename);
    if (content_type) {
        size += strlen("Content-Type: \r\n") + strlen(content_type);
    }
    size += 2; /* \r\n */
    size += data_size;
    size += 2; /* \r\n */
    return size;
}

dc_status_t dc_multipart_init(dc_multipart_t* mp) {
    if (!mp) return DC_ERROR_NULL_POINTER;
    memset(mp, 0, sizeof(*mp));
    dc_status_t st = dc_string_init(&mp->boundary);
    if (st != DC_OK) return st;
    st = dc_string_init(&mp->body);
    if (st != DC_OK) {
        dc_string_free(&mp->boundary);
        return st;
    }
    st = dc_multipart_set_default_boundary(mp);
    if (st != DC_OK) {
        dc_string_free(&mp->boundary);
        dc_string_free(&mp->body);
        return st;
    }
    return DC_OK;
}

void dc_multipart_free(dc_multipart_t* mp) {
    if (!mp) return;
    dc_string_free(&mp->boundary);
    dc_string_free(&mp->body);
    memset(mp, 0, sizeof(*mp));
}

dc_status_t dc_multipart_set_boundary(dc_multipart_t* mp, const char* boundary) {
    if (!mp || !boundary) return DC_ERROR_NULL_POINTER;
    if (boundary[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (!dc_multipart_value_is_valid(boundary)) return DC_ERROR_INVALID_PARAM;
    if (mp->part_count > 0) return DC_ERROR_INVALID_STATE;
    return dc_string_set_cstr(&mp->boundary, boundary);
}

dc_status_t dc_multipart_set_limits(dc_multipart_t* mp, size_t max_file_size, size_t max_total_size) {
    if (!mp) return DC_ERROR_NULL_POINTER;
    mp->max_file_size = max_file_size;
    mp->max_total_size = max_total_size;
    return DC_OK;
}

dc_status_t dc_multipart_get_content_type(const dc_multipart_t* mp, dc_string_t* out) {
    if (!mp || !out) return DC_ERROR_NULL_POINTER;
    if (dc_string_is_empty(&mp->boundary)) return DC_ERROR_INVALID_PARAM;
    return dc_string_printf(out, "multipart/form-data; boundary=%s", dc_string_cstr(&mp->boundary));
}

dc_status_t dc_multipart_add_field(dc_multipart_t* mp, const char* name, const char* value) {
    if (!mp || !name || !value) return DC_ERROR_NULL_POINTER;
    if (mp->finalized) return DC_ERROR_INVALID_STATE;
    if (name[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (!dc_multipart_value_is_valid(name)) return DC_ERROR_INVALID_PARAM;

    const char* boundary = dc_string_cstr(&mp->boundary);
    size_t part_size = dc_multipart_estimate_simple_part(boundary, name, value, NULL);
    if (mp->max_total_size > 0 && (mp->total_size + part_size) > mp->max_total_size) {
        return DC_ERROR_INVALID_PARAM;
    }

    dc_status_t st = dc_multipart_append_boundary(mp);
    if (st != DC_OK) return st;
    st = dc_string_append_printf(&mp->body,
                                 "Content-Disposition: form-data; name=\"%s\"\r\n\r\n",
                                 name);
    if (st != DC_OK) return st;
    st = dc_string_append_cstr(&mp->body, value);
    if (st != DC_OK) return st;
    st = dc_string_append_cstr(&mp->body, "\r\n");
    if (st != DC_OK) return st;

    mp->part_count++;
    mp->total_size = dc_string_length(&mp->body);
    return DC_OK;
}

dc_status_t dc_multipart_add_payload_json(dc_multipart_t* mp, const char* json) {
    if (!mp || !json) return DC_ERROR_NULL_POINTER;
    if (mp->finalized) return DC_ERROR_INVALID_STATE;
    if (json[0] == '\0') return DC_ERROR_INVALID_PARAM;

    const char* boundary = dc_string_cstr(&mp->boundary);
    size_t part_size = dc_multipart_estimate_simple_part(boundary, "payload_json", json, "application/json");
    if (mp->max_total_size > 0 && (mp->total_size + part_size) > mp->max_total_size) {
        return DC_ERROR_INVALID_PARAM;
    }

    dc_status_t st = dc_multipart_append_boundary(mp);
    if (st != DC_OK) return st;
    st = dc_string_append_cstr(&mp->body,
                               "Content-Disposition: form-data; name=\"payload_json\"\r\n");
    if (st != DC_OK) return st;
    st = dc_string_append_cstr(&mp->body, "Content-Type: application/json\r\n\r\n");
    if (st != DC_OK) return st;
    st = dc_string_append_cstr(&mp->body, json);
    if (st != DC_OK) return st;
    st = dc_string_append_cstr(&mp->body, "\r\n");
    if (st != DC_OK) return st;

    mp->part_count++;
    mp->total_size = dc_string_length(&mp->body);
    return DC_OK;
}

dc_status_t dc_multipart_add_file_named(dc_multipart_t* mp, const char* field_name,
                                        const char* filename,
                                        const void* data, size_t size,
                                        const char* content_type) {
    if (!mp || !field_name || !filename || (!data && size > 0)) return DC_ERROR_NULL_POINTER;
    if (mp->finalized) return DC_ERROR_INVALID_STATE;
    if (field_name[0] == '\0' || filename[0] == '\0') return DC_ERROR_INVALID_PARAM;
    if (!dc_multipart_value_is_valid(field_name) || !dc_multipart_value_is_valid(filename)) {
        return DC_ERROR_INVALID_PARAM;
    }
    if (content_type && content_type[0] != '\0' && !dc_multipart_value_is_valid(content_type)) {
        return DC_ERROR_INVALID_PARAM;
    }
    if (!dc_attachment_filename_is_valid(filename)) return DC_ERROR_INVALID_PARAM;
    if (!dc_attachment_size_is_valid(size, mp->max_file_size)) return DC_ERROR_INVALID_PARAM;

    const char* boundary = dc_string_cstr(&mp->boundary);
    size_t part_size = dc_multipart_estimate_file_part(boundary, field_name, filename, size, content_type);
    if (mp->max_total_size > 0 && (mp->total_size + part_size) > mp->max_total_size) {
        return DC_ERROR_INVALID_PARAM;
    }

    dc_status_t st = dc_multipart_append_boundary(mp);
    if (st != DC_OK) return st;
    st = dc_string_append_printf(&mp->body,
                                 "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n",
                                 field_name, filename);
    if (st != DC_OK) return st;
    if (content_type && content_type[0] != '\0') {
        st = dc_string_append_printf(&mp->body, "Content-Type: %s\r\n", content_type);
        if (st != DC_OK) return st;
    }
    st = dc_string_append_cstr(&mp->body, "\r\n");
    if (st != DC_OK) return st;
    if (size > 0) {
        st = dc_string_append_buffer(&mp->body, (const char*)data, size);
        if (st != DC_OK) return st;
    }
    st = dc_string_append_cstr(&mp->body, "\r\n");
    if (st != DC_OK) return st;

    mp->part_count++;
    mp->total_size = dc_string_length(&mp->body);
    return DC_OK;
}

dc_status_t dc_multipart_add_file(dc_multipart_t* mp, const char* filename,
                                  const void* data, size_t size,
                                  const char* content_type,
                                  size_t* out_index) {
    if (!mp || !filename || (!data && size > 0)) return DC_ERROR_NULL_POINTER;
    size_t index = mp->file_count;
    char field_name[32];
    int written = snprintf(field_name, sizeof(field_name), "files[%zu]", index);
    if (written < 0 || (size_t)written >= sizeof(field_name)) {
        return DC_ERROR_BUFFER_TOO_SMALL;
    }

    dc_status_t st = dc_multipart_add_file_named(mp, field_name, filename, data, size, content_type);
    if (st != DC_OK) return st;

    mp->file_count++;
    if (out_index) *out_index = index;
    return DC_OK;
}

dc_status_t dc_multipart_finish(dc_multipart_t* mp) {
    if (!mp) return DC_ERROR_NULL_POINTER;
    if (mp->finalized) return DC_OK;
    if (dc_string_is_empty(&mp->boundary)) return DC_ERROR_INVALID_PARAM;

    size_t closing_size = 2 + dc_string_length(&mp->boundary) + 4; /* --boundary--\r\n */
    if (mp->max_total_size > 0 && (mp->total_size + closing_size) > mp->max_total_size) {
        return DC_ERROR_INVALID_PARAM;
    }

    dc_status_t st = dc_string_append_cstr(&mp->body, "--");
    if (st != DC_OK) return st;
    st = dc_string_append_string(&mp->body, &mp->boundary);
    if (st != DC_OK) return st;
    st = dc_string_append_cstr(&mp->body, "--\r\n");
    if (st != DC_OK) return st;

    mp->finalized = 1;
    mp->total_size = dc_string_length(&mp->body);
    return DC_OK;
}
