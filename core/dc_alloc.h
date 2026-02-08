#ifndef DC_ALLOC_H
#define DC_ALLOC_H

/**
 * @file dc_alloc.h
 * @brief Memory allocation wrappers with explicit hooks and safety checks
 */

#include <stddef.h>
#include "dc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Memory allocation function pointer type
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory or NULL on failure
 */
typedef void* (*dc_alloc_fn_t)(size_t size);

/**
 * @brief Memory reallocation function pointer type
 * @param ptr Pointer to existing memory or NULL
 * @param size New size in bytes
 * @return Pointer to reallocated memory or NULL on failure
 */
typedef void* (*dc_realloc_fn_t)(void* ptr, size_t size);

/**
 * @brief Memory deallocation function pointer type
 * @param ptr Pointer to memory to free
 */
typedef void (*dc_free_fn_t)(void* ptr);

/**
 * @brief Memory allocation hooks
 */
typedef struct {
    dc_alloc_fn_t alloc;     /**< Allocation function */
    dc_realloc_fn_t realloc; /**< Reallocation function */
    dc_free_fn_t free;       /**< Deallocation function */
} dc_alloc_hooks_t;

/**
 * @brief Set custom memory allocation hooks
 * @param hooks Pointer to allocation hooks structure
 * @return DC_OK on success, error code on failure
 * 
 * @note All three function pointers must be non-NULL
 * @note This function is not thread-safe
 */
dc_status_t dc_alloc_set_hooks(const dc_alloc_hooks_t* hooks);

/**
 * @brief Get current memory allocation hooks
 * @param hooks Pointer to store current hooks
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_alloc_get_hooks(dc_alloc_hooks_t* hooks);

/**
 * @brief Reset to default memory allocation hooks (malloc/realloc/free)
 */
void dc_alloc_reset_hooks(void);

/**
 * @brief Allocate memory with NULL check
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory or NULL on failure
 * 
 * @note Memory is not initialized
 * @note Returns NULL if size is 0
 */
void* dc_alloc(size_t size);

/**
 * @brief Allocate zero-initialized memory
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to allocated memory or NULL on failure
 * 
 * @note Memory is zero-initialized
 * @note Returns NULL if count or size is 0
 */
void* dc_calloc(size_t count, size_t size);

/**
 * @brief Reallocate memory with NULL check
 * @param ptr Pointer to existing memory or NULL
 * @param size New size in bytes
 * @return Pointer to reallocated memory or NULL on failure
 * 
 * @note If ptr is NULL, behaves like dc_alloc
 * @note If size is 0, behaves like dc_free and returns NULL
 */
void* dc_realloc(void* ptr, size_t size);

/**
 * @brief Free memory with NULL check
 * @param ptr Pointer to memory to free
 * 
 * @note Safe to call with NULL pointer
 */
void dc_free(void* ptr);

/**
 * @brief Duplicate a string with proper memory management
 * @param str String to duplicate
 * @return Newly allocated string or NULL on failure
 * 
 * @note Caller must free the returned string with dc_free
 * @note Returns NULL if str is NULL
 */
char* dc_strdup(const char* str);

/**
 * @brief Duplicate a string with length limit
 * @param str String to duplicate
 * @param max_len Maximum length to copy
 * @return Newly allocated string or NULL on failure
 * 
 * @note Caller must free the returned string with dc_free
 * @note Returns NULL if str is NULL
 * @note Result is always null-terminated
 */
char* dc_strndup(const char* str, size_t max_len);

/**
 * @brief Allocate aligned memory (alignment must be power of two)
 * @param size Number of bytes to allocate
 * @param alignment Alignment in bytes
 * @return Pointer to allocated memory or NULL on failure
 *
 * @note Caller must free the returned pointer with dc_free when supported.
 * @note Returns NULL if alignment is invalid or not supported on this platform.
 */
void* dc_alloc_aligned(size_t size, size_t alignment);

#ifdef __cplusplus
}
#endif

#endif /* DC_ALLOC_H */
