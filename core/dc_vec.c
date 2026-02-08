/**
 * @file dc_vec.c
 * @brief dynamic array implementation
 */

#include "dc_vec.h"
#include "dc_alloc.h"
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <stdalign.h>

/* Compiler hints for branch prediction (GCC/Clang/MSVC) */
#if defined(__GNUC__) || defined(__clang__)
    #define DC_LIKELY(x)   __builtin_expect((long)!!(x), 1L)
    #define DC_UNLIKELY(x) __builtin_expect((long)!!(x), 0L)
#elif defined(_MSC_VER)
    #define DC_LIKELY(x)   (x)
    #define DC_UNLIKELY(x) (x)
#else
    #define DC_LIKELY(x)   (x)
    #define DC_UNLIKELY(x) (x)
#endif

/* Optional secure memory clearing (prevent optimization). */
#ifndef DC_VEC_SECURE_CLEAR
#define DC_VEC_SECURE_CLEAR 0
#endif

#if DC_VEC_SECURE_CLEAR
static void dc_secure_zero(void* ptr, size_t len) {
    if (ptr && len > 0) {
        volatile unsigned char* p = (volatile unsigned char*)ptr;
        while (len--) *p++ = 0;
    }
}
#endif

/* Safe size calculation with overflow detection */
static dc_status_t dc_safe_multiply(size_t a, size_t b, size_t* result) {
    if (DC_UNLIKELY(a == 0 || b == 0)) {
        *result = 0;
        return DC_OK;
    }
    if (DC_UNLIKELY(a > SIZE_MAX / b)) {
        return DC_ERROR_INVALID_PARAM;
    }
    *result = a * b;
    return DC_OK;
}

/**
 * @brief Ensures capacity with 2x growth for stable amortized O(1) push
 */
static dc_status_t dc_vec_ensure_capacity(dc_vec_t* vec, size_t min_capacity) {
    if (DC_UNLIKELY(!vec)) return DC_ERROR_NULL_POINTER;
    if (DC_LIKELY(min_capacity <= vec->capacity)) return DC_OK;
    if (DC_UNLIKELY(vec->element_size == 0)) return DC_ERROR_INVALID_PARAM;

    /* Prevent overflow in capacity calculations */
    if (DC_UNLIKELY(min_capacity > SIZE_MAX / vec->element_size)) {
        return DC_ERROR_INVALID_PARAM;
    }

    /* Start with a small power-of-2, then grow by 2x */
    size_t new_cap = vec->capacity > 0 ? vec->capacity : 8;
    while (DC_UNLIKELY(new_cap < min_capacity)) {
        if (DC_UNLIKELY(new_cap > SIZE_MAX / 2)) {
            new_cap = min_capacity;
            break;
        }
        new_cap *= 2;
    }

    /* Final overflow check before allocation */
    size_t new_bytes;
    dc_status_t status = dc_safe_multiply(new_cap, vec->element_size, &new_bytes);
    if (DC_UNLIKELY(status != DC_OK)) return status;

    void* new_data = dc_realloc(vec->data, new_bytes);
    if (DC_UNLIKELY(!new_data)) return DC_ERROR_OUT_OF_MEMORY;

    vec->data = new_data;
    vec->capacity = new_cap;
    return DC_OK;
}

dc_status_t dc_vec_init(dc_vec_t* vec, size_t element_size) {
    if (DC_UNLIKELY(!vec || element_size == 0)) return DC_ERROR_INVALID_PARAM;
    
    vec->data = NULL;
    vec->length = 0;
    vec->capacity = 0;
    vec->element_size = element_size;
    return DC_OK;
}

dc_status_t dc_vec_init_with_capacity(dc_vec_t* vec, size_t element_size, size_t capacity) {
    if (DC_UNLIKELY(!vec || element_size == 0)) return DC_ERROR_INVALID_PARAM;

    if (DC_UNLIKELY(capacity > 0)) {
        size_t bytes;
        dc_status_t status = dc_safe_multiply(capacity, element_size, &bytes);
        if (DC_UNLIKELY(status != DC_OK)) return status;

        vec->data = dc_alloc(bytes);
        if (DC_UNLIKELY(!vec->data)) return DC_ERROR_OUT_OF_MEMORY;
    } else {
        vec->data = NULL;
    }
    
    vec->length = 0;
    vec->capacity = capacity;
    vec->element_size = element_size;
    return DC_OK;
}

void dc_vec_free(dc_vec_t* vec) {
    if (DC_LIKELY(vec)) {
#if DC_VEC_SECURE_CLEAR
        if (vec->data && vec->element_size > 0 && vec->length > 0) {
            size_t bytes;
            if (dc_safe_multiply(vec->length, vec->element_size, &bytes) == DC_OK) {
                dc_secure_zero(vec->data, bytes);
            }
        }
#endif
        dc_free(vec->data);
        vec->data = NULL;
        vec->length = 0;
        vec->capacity = 0;
        vec->element_size = 0;
    }
}

dc_status_t dc_vec_push(dc_vec_t* vec, const void* element) {
    if (DC_UNLIKELY(!vec || !element)) return DC_ERROR_NULL_POINTER;
    
    if (DC_UNLIKELY(vec->length >= vec->capacity)) {
        dc_status_t status = dc_vec_ensure_capacity(vec, vec->length + 1);
        if (DC_UNLIKELY(status != DC_OK)) return status;
    }
    
    /* Use uintptr_t for safe pointer arithmetic without strict aliasing violation */
    uintptr_t base = (uintptr_t)vec->data;
    void* dest = (void*)(base + (vec->length * vec->element_size));
    
    memcpy(dest, element, vec->element_size);
    vec->length++;
    return DC_OK;
}

void* dc_vec_at(const dc_vec_t* vec, size_t index) {
    if (DC_UNLIKELY(!vec || index >= vec->length)) return NULL;
    return (void*)((uintptr_t)vec->data + (index * vec->element_size));
}

size_t dc_vec_length(const dc_vec_t* vec) {
    return DC_LIKELY(vec) ? vec->length : 0;
}

size_t dc_vec_capacity(const dc_vec_t* vec) {
    return DC_LIKELY(vec) ? vec->capacity : 0;
}

int dc_vec_is_empty(const dc_vec_t* vec) {
    return DC_UNLIKELY(!vec) || vec->length == 0;
}

dc_status_t dc_vec_clear(dc_vec_t* vec) {
    if (DC_UNLIKELY(!vec)) return DC_ERROR_NULL_POINTER;
    
#if DC_VEC_SECURE_CLEAR
    if (vec->data && vec->element_size > 0) {
        size_t bytes;
        if (dc_safe_multiply(vec->length, vec->element_size, &bytes) == DC_OK) {
            dc_secure_zero(vec->data, bytes);
        }
    }
#endif
    
    vec->length = 0;
    return DC_OK;
}

dc_status_t dc_vec_reserve(dc_vec_t* vec, size_t capacity) {
    if (DC_UNLIKELY(!vec)) return DC_ERROR_NULL_POINTER;
    if (DC_UNLIKELY(capacity <= vec->capacity)) return DC_OK;
    return dc_vec_ensure_capacity(vec, capacity);
}

dc_status_t dc_vec_shrink_to_fit(dc_vec_t* vec) {
    if (DC_UNLIKELY(!vec)) return DC_ERROR_NULL_POINTER;
    
    if (vec->length == 0) {
        if (vec->data) {
            dc_free(vec->data);
            vec->data = NULL;
        }
        vec->capacity = 0;
        return DC_OK;
    }

    if (vec->length == vec->capacity) return DC_OK;
    
    size_t new_bytes;
    dc_status_t status = dc_safe_multiply(vec->length, vec->element_size, &new_bytes);
    if (DC_UNLIKELY(status != DC_OK)) return status;
    
    void* new_data = dc_realloc(vec->data, new_bytes);
    if (DC_UNLIKELY(!new_data)) return DC_ERROR_OUT_OF_MEMORY;
    
    vec->data = new_data;
    vec->capacity = vec->length;
    return DC_OK;
}

dc_status_t dc_vec_pop(dc_vec_t* vec, void* element) {
    if (DC_UNLIKELY(!vec)) return DC_ERROR_NULL_POINTER;
    if (DC_UNLIKELY(vec->length == 0)) return DC_ERROR_NOT_FOUND;

    vec->length--;
    if (element) {
        memcpy(element, (void*)((uintptr_t)vec->data + vec->length * vec->element_size), 
               vec->element_size);
    }
    return DC_OK;
}

/**
 * @brief O(1) unordered removal (swap with last element)
 * @note Faster than dc_vec_remove for unordered collections
 */
dc_status_t dc_vec_swap_remove(dc_vec_t* vec, size_t index, void* element) {
    if (DC_UNLIKELY(!vec)) return DC_ERROR_NULL_POINTER;
    if (DC_UNLIKELY(index >= vec->length)) return DC_ERROR_INVALID_PARAM;

    void* target = (void*)((uintptr_t)vec->data + index * vec->element_size);
    if (element) {
        memcpy(element, target, vec->element_size);
    }
    
    /* Swap with last element if not already last */
    if (index != vec->length - 1) {
        void* last = (void*)((uintptr_t)vec->data + (vec->length - 1) * vec->element_size);
        memcpy(target, last, vec->element_size);
    }
    
    vec->length--;
    return DC_OK;
}

dc_status_t dc_vec_insert(dc_vec_t* vec, size_t index, const void* element) {
    if (DC_UNLIKELY(!vec || !element)) return DC_ERROR_NULL_POINTER;
    if (DC_UNLIKELY(index > vec->length)) return DC_ERROR_INVALID_PARAM;

    dc_status_t status = dc_vec_ensure_capacity(vec, vec->length + 1);
    if (DC_UNLIKELY(status != DC_OK)) return status;

    if (index < vec->length) {
        void* dest = (void*)((uintptr_t)vec->data + (index + 1) * vec->element_size);
        void* src = (void*)((uintptr_t)vec->data + index * vec->element_size);
        size_t move_bytes;
        
        status = dc_safe_multiply(vec->length - index, vec->element_size, &move_bytes);
        if (DC_UNLIKELY(status != DC_OK)) return status;
        
        memmove(dest, src, move_bytes);
    }

    memcpy((void*)((uintptr_t)vec->data + index * vec->element_size), element, vec->element_size);
    vec->length++;
    return DC_OK;
}

dc_status_t dc_vec_remove(dc_vec_t* vec, size_t index, void* element) {
    if (DC_UNLIKELY(!vec)) return DC_ERROR_NULL_POINTER;
    if (DC_UNLIKELY(index >= vec->length)) return DC_ERROR_INVALID_PARAM;

    void* target = (void*)((uintptr_t)vec->data + index * vec->element_size);
    
    if (element) {
        memcpy(element, target, vec->element_size);
    }

    if (index < vec->length - 1) {
        void* dest = target;
        void* src = (void*)((uintptr_t)vec->data + (index + 1) * vec->element_size);
        size_t move_bytes;
        
        dc_status_t status = dc_safe_multiply(vec->length - index - 1, vec->element_size, &move_bytes);
        if (DC_UNLIKELY(status != DC_OK)) return status;
        
        memmove(dest, src, move_bytes);
    }

    vec->length--;
    return DC_OK;
}

dc_status_t dc_vec_get(const dc_vec_t* vec, size_t index, void* element) {
    if (DC_UNLIKELY(!vec || !element)) return DC_ERROR_NULL_POINTER;
    if (DC_UNLIKELY(index >= vec->length)) return DC_ERROR_INVALID_PARAM;
    
    memcpy(element, (void*)((uintptr_t)vec->data + index * vec->element_size), vec->element_size);
    return DC_OK;
}

dc_status_t dc_vec_set(dc_vec_t* vec, size_t index, const void* element) {
    if (DC_UNLIKELY(!vec || !element)) return DC_ERROR_NULL_POINTER;
    if (DC_UNLIKELY(index >= vec->length)) return DC_ERROR_INVALID_PARAM;
    
    memcpy((void*)((uintptr_t)vec->data + index * vec->element_size), element, vec->element_size);
    return DC_OK;
}

void* dc_vec_front(const dc_vec_t* vec) { 
    return dc_vec_at(vec, (size_t)0); 
}

void* dc_vec_back(const dc_vec_t* vec) { 
    return (DC_LIKELY(vec) && vec->length > 0) ? 
           dc_vec_at(vec, vec->length - 1) : NULL; 
}

void* dc_vec_data(const dc_vec_t* vec) { 
    return DC_LIKELY(vec) ? vec->data : NULL; 
}

dc_status_t dc_vec_resize(dc_vec_t* vec, size_t new_length) {
    if (DC_UNLIKELY(!vec)) return DC_ERROR_NULL_POINTER;
    
    if (new_length > vec->length) {
        dc_status_t status = dc_vec_ensure_capacity(vec, new_length);
        if (DC_UNLIKELY(status != DC_OK)) return status;

        /* Zero-initialize new elements to match API contract */
        size_t zero_bytes;
        dc_status_t zst = dc_safe_multiply(new_length - vec->length, vec->element_size, &zero_bytes);
        if (DC_UNLIKELY(zst != DC_OK)) return zst;
        memset((void*)((uintptr_t)vec->data + vec->length * vec->element_size), 0, zero_bytes);
    } else if (new_length < vec->length) {
#if DC_VEC_SECURE_CLEAR
        if (vec->element_size > 0) {
            size_t clear_bytes;
            dc_status_t status = dc_safe_multiply(vec->length - new_length, vec->element_size, &clear_bytes);
            if (status == DC_OK) {
                dc_secure_zero((void*)((uintptr_t)vec->data + new_length * vec->element_size), clear_bytes);
            }
        }
#endif
    }
    
    vec->length = new_length;
    return DC_OK;
}

dc_status_t dc_vec_find(const dc_vec_t* vec, const void* element, 
                        int (*compare)(const void*, const void*), size_t* index) {
    if (DC_UNLIKELY(!vec || !element)) return DC_ERROR_NULL_POINTER;

    for (size_t i = 0; i < vec->length; i++) {
        void* current = (void*)((uintptr_t)vec->data + i * vec->element_size);
        int cmp;
        
        if (compare) {
            cmp = compare(current, element);
        } else {
            /* memcmp is unsafe for structs with padding - document this limitation */
            cmp = memcmp(current, element, vec->element_size);
        }
        
        if (cmp == 0) {
            if (index) *index = i;
            return DC_OK;
        }
    }
    return DC_ERROR_NOT_FOUND;
}

/* High-performance batch operations */

dc_status_t dc_vec_append(dc_vec_t* vec, const void* elements, size_t count) {
    if (DC_UNLIKELY(!vec || (!elements && count > 0))) return DC_ERROR_NULL_POINTER;
    if (count == 0) return DC_OK;

    if (DC_UNLIKELY(count > SIZE_MAX - vec->length)) return DC_ERROR_INVALID_PARAM;
    
    /* Check for overflow in total size */
    size_t add_bytes;
    dc_status_t status = dc_safe_multiply(count, vec->element_size, &add_bytes);
    if (DC_UNLIKELY(status != DC_OK)) return status;
    
    if (DC_UNLIKELY(vec->length + count > vec->capacity)) {
        status = dc_vec_ensure_capacity(vec, vec->length + count);
        if (DC_UNLIKELY(status != DC_OK)) return status;
    }
    
    memcpy((void*)((uintptr_t)vec->data + vec->length * vec->element_size), 
           elements, add_bytes);
    vec->length += count;
    return DC_OK;
}

dc_status_t dc_vec_copy(dc_vec_t* dst, const dc_vec_t* src) {
    if (DC_UNLIKELY(!dst || !src)) return DC_ERROR_NULL_POINTER;
    if (DC_UNLIKELY(dst == src)) return DC_OK;
    
    dc_vec_free(dst);
    
    dc_status_t status = dc_vec_init_with_capacity(dst, src->element_size, src->length);
    if (DC_UNLIKELY(status != DC_OK)) return status;
    
    if (src->length > 0) {
        size_t bytes;
        status = dc_safe_multiply(src->length, src->element_size, &bytes);
        if (DC_UNLIKELY(status != DC_OK)) {
            dc_vec_free(dst);
            return status;
        }
        memcpy(dst->data, src->data, bytes);
        dst->length = src->length;
    }
    
    return DC_OK;
}

void dc_vec_swap(dc_vec_t* a, dc_vec_t* b) {
    if (DC_UNLIKELY(!a || !b || a == b)) return;
    
    dc_vec_t tmp = *a;
    *a = *b;
    *b = tmp;
}
