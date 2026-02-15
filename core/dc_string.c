/**
 * @file dc_string.c
 * @brief Safe string implementation
 */

#include "dc_string.h"
#include "dc_alloc.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>

static void dc_secure_zero(void* ptr, size_t len) {
    if (!ptr || len == 0) return;
    volatile unsigned char* p = (volatile unsigned char*)ptr;
    for (size_t i = 0; i < len; i++) {
        p[i] = 0;
    }
}

static dc_status_t dc_string_ensure_capacity(dc_string_t* str, size_t min_capacity) {
    if (!str) return DC_ERROR_NULL_POINTER;
    if (min_capacity == 0) return DC_OK;

    if (str->capacity >= min_capacity) {
        return DC_OK;
    }

    size_t new_cap = str->capacity > 0 ? str->capacity : 16;
    while (new_cap < min_capacity) {
        if (new_cap > SIZE_MAX / 2) {
            new_cap = min_capacity;
            break;
        }
        new_cap *= 2;
    }

    if (new_cap < min_capacity) {
        return DC_ERROR_INVALID_PARAM;
    }

    char* new_data = dc_realloc(str->data, new_cap);
    if (!new_data && new_cap > 0) {
        return DC_ERROR_OUT_OF_MEMORY;
    }

    str->data = new_data;
    str->capacity = new_cap;
    if (str->length == 0 && str->data) {
        str->data[0] = '\0';
    }
    return DC_OK;
}

dc_status_t dc_string_init(dc_string_t* str) {
    if (!str) return DC_ERROR_NULL_POINTER;
    
    str->data = NULL;
    str->length = 0;
    str->capacity = 0;
    return DC_OK;
}

dc_status_t dc_string_init_with_capacity(dc_string_t* str, size_t capacity) {
    if (!str) return DC_ERROR_NULL_POINTER;
    str->data = dc_alloc(capacity);
    if (!str->data && capacity > 0) return DC_ERROR_OUT_OF_MEMORY;
    
    str->length = 0;
    str->capacity = capacity;
    if (capacity > 0) str->data[0] = '\0';
    return DC_OK;
}

dc_status_t dc_string_init_from_cstr(dc_string_t* str, const char* cstr) {
    if (!str) return DC_ERROR_NULL_POINTER;
    if (!cstr) return dc_string_init(str);
    
    size_t len = strlen(cstr);
    if (len > SIZE_MAX - 1) return DC_ERROR_INVALID_PARAM;
    dc_status_t status = dc_string_init_with_capacity(str, len + 1);
    if (status != DC_OK) return status;
    
    memcpy(str->data, cstr, len + 1);
    str->length = len;
    return DC_OK;
}

dc_status_t dc_string_init_from_buffer(dc_string_t* str, const char* data, size_t length) {
    if (!str) return DC_ERROR_NULL_POINTER;
    if (!data && length > 0) return DC_ERROR_NULL_POINTER;
    if (length > SIZE_MAX - 1) return DC_ERROR_INVALID_PARAM;
    
    dc_status_t status = dc_string_init_with_capacity(str, length + 1);
    if (status != DC_OK) return status;
    
    if (length > 0) {
        memcpy(str->data, data, length);
    }
    str->data[length] = '\0';
    str->length = length;
    return DC_OK;
}

void dc_string_free(dc_string_t* str) {
    if (str) {
        if (str->data && str->capacity > 0) {
            dc_secure_zero(str->data, str->capacity);
        }
        dc_free(str->data);
        str->data = NULL;
        str->length = 0;
        str->capacity = 0;
    }
}

dc_status_t dc_string_clear(dc_string_t* str) {
    if (!str) return DC_ERROR_NULL_POINTER;
    
    str->length = 0;
    if (str->data && str->capacity > 0) {
        str->data[0] = '\0';
    }
    return DC_OK;
}

dc_status_t dc_string_append_cstr(dc_string_t* str, const char* cstr) {
    if (!str || !cstr) return DC_ERROR_NULL_POINTER;
    
    return dc_string_append_buffer(str, cstr, strlen(cstr));
}

const char* dc_string_cstr(const dc_string_t* str) {
    return (str && str->data) ? str->data : "";
}

size_t dc_string_length(const dc_string_t* str) {
    return str ? str->length : 0;
}

dc_status_t dc_string_reserve(dc_string_t* str, size_t capacity) {
    if (!str) return DC_ERROR_NULL_POINTER;
    if (capacity == 0) return DC_OK;
    return dc_string_ensure_capacity(str, capacity);
}

dc_status_t dc_string_shrink_to_fit(dc_string_t* str) {
    if (!str) return DC_ERROR_NULL_POINTER;
    if (str->length == 0) {
        dc_free(str->data);
        str->data = NULL;
        str->capacity = 0;
        return DC_OK;
    }

    size_t new_cap = str->length + 1;
    if (new_cap == str->capacity) return DC_OK;

    char* new_data = dc_realloc(str->data, new_cap);
    if (!new_data) return DC_ERROR_OUT_OF_MEMORY;

    str->data = new_data;
    str->capacity = new_cap;
    return DC_OK;
}

dc_status_t dc_string_append_buffer(dc_string_t* str, const char* data, size_t length) {
    if (!str || (!data && length > 0)) return DC_ERROR_NULL_POINTER;
    if (length == 0) return DC_OK;
    if ((str->capacity == 0 && str->length > 0) ||
        (str->capacity > 0 && str->length >= str->capacity)) {
        return DC_ERROR_INVALID_PARAM;
    }

    if (length > SIZE_MAX - str->length - 1) {
        return DC_ERROR_INVALID_PARAM;
    }

    size_t new_len = str->length + length;
    const char* src = data;
    int overlap = 0;
    size_t offset = 0;
    if (str->data && data && str->capacity > 0) {
        const char* start = str->data;
        const char* end = str->data + str->capacity;
        if (data >= start && data < end) {
            overlap = 1;
            offset = (size_t)(data - start);
        }
    }

    dc_status_t status = dc_string_ensure_capacity(str, new_len + 1);
    if (status != DC_OK) return status;

    if (overlap) {
        src = str->data + offset;
        memmove(str->data + str->length, src, length);
    } else {
        memcpy(str->data + str->length, data, length);
    }
    str->data[new_len] = '\0';
    str->length = new_len;
    return DC_OK;
}

dc_status_t dc_string_append_char(dc_string_t* str, char c) {
    return dc_string_append_buffer(str, &c, (size_t)1);
}

dc_status_t dc_string_append_string(dc_string_t* str, const dc_string_t* other) {
    if (!other) return DC_ERROR_NULL_POINTER;
    return dc_string_append_buffer(str, other->data, other->length);
}

dc_status_t dc_string_append_printf(dc_string_t* str, const char* format, ...) {
    if (!str || !format) return DC_ERROR_NULL_POINTER;
    va_list args;
    va_start(args, format);
    dc_status_t status = dc_string_append_vprintf(str, format, args);
    va_end(args);
    return status;
}

dc_status_t dc_string_append_vprintf(dc_string_t* str, const char* format, va_list args) {
    if (!str || !format) return DC_ERROR_NULL_POINTER;
    if ((str->capacity == 0 && str->length > 0) ||
        (str->capacity > 0 && str->length >= str->capacity)) {
        return DC_ERROR_INVALID_PARAM;
    }

    va_list args_copy;
    va_copy(args_copy, args);
    int required = vsnprintf(NULL, (size_t)0, format, args_copy);
    va_end(args_copy);
    if (required < 0) return DC_ERROR_INVALID_FORMAT;

    size_t needed = (size_t)required;
    if (needed > SIZE_MAX - str->length - 1) {
        return DC_ERROR_INVALID_PARAM;
    }
    dc_status_t status = dc_string_ensure_capacity(str, str->length + needed + 1);
    if (status != DC_OK) return status;
    va_copy(args_copy, args);
    int written = vsnprintf(str->data + str->length, needed + 1, format, args_copy);
    va_end(args_copy);
    if (written < 0 || (size_t)written != needed) return DC_ERROR_INVALID_FORMAT;
    str->length += (size_t)written;
    return DC_OK;
}

dc_status_t dc_string_set_cstr(dc_string_t* str, const char* cstr) {
    if (!str) return DC_ERROR_NULL_POINTER;
    dc_status_t status = dc_string_clear(str);
    if (status != DC_OK) return status;
    if (!cstr) return DC_OK;
    return dc_string_append_buffer(str, cstr, strlen(cstr));
}

dc_status_t dc_string_set_buffer(dc_string_t* str, const char* data, size_t length) {
    if (!str || (!data && length > 0)) return DC_ERROR_NULL_POINTER;
    dc_status_t status = dc_string_clear(str);
    if (status != DC_OK) return status;
    return dc_string_append_buffer(str, data, length);
}

dc_status_t dc_string_printf(dc_string_t* str, const char* format, ...) {
    if (!str || !format) return DC_ERROR_NULL_POINTER;
    dc_status_t status = dc_string_clear(str);
    if (status != DC_OK) return status;
    va_list args;
    va_start(args, format);
    status = dc_string_append_vprintf(str, format, args);
    va_end(args);
    return status;
}

dc_status_t dc_string_vprintf(dc_string_t* str, const char* format, va_list args) {
    if (!str || !format) return DC_ERROR_NULL_POINTER;
    dc_status_t status = dc_string_clear(str);
    if (status != DC_OK) return status;
    return dc_string_append_vprintf(str, format, args);
}

size_t dc_string_capacity(const dc_string_t* str) { return str ? str->capacity : 0; }
int dc_string_is_empty(const dc_string_t* str) { return !str || str->length == 0; }
int dc_string_compare_cstr(const dc_string_t* str, const char* cstr) {
    const char* a = (str && str->data) ? str->data : "";
    const char* b = cstr ? cstr : "";
    return strcmp(a, b);
}
int dc_string_compare(const dc_string_t* str1, const dc_string_t* str2) {
    const char* a = (str1 && str1->data) ? str1->data : "";
    const char* b = (str2 && str2->data) ? str2->data : "";
    return strcmp(a, b);
}
