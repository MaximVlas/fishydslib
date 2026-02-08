#ifndef DC_VEC_H
#define DC_VEC_H

/**
 * @file dc_vec.h
 * @brief Safe dynamic array helpers with bounds checking
 */

#include <stddef.h>
#include "dc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dynamic array structure
 */
typedef struct {
    void* data;           /**< Array data */
    size_t length;        /**< Current number of elements */
    size_t capacity;      /**< Total capacity */
    size_t element_size;  /**< Size of each element */
} dc_vec_t;

/**
 * @brief Initialize empty vector
 * @param vec Pointer to vector structure
 * @param element_size Size of each element
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_init(dc_vec_t* vec, size_t element_size);

/**
 * @brief Initialize vector with initial capacity
 * @param vec Pointer to vector structure
 * @param element_size Size of each element
 * @param capacity Initial capacity
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_init_with_capacity(dc_vec_t* vec, size_t element_size, size_t capacity);

/**
 * @brief Free vector resources
 * @param vec Pointer to vector structure
 */
void dc_vec_free(dc_vec_t* vec);

/**
 * @brief Clear vector content (keep capacity)
 * @param vec Pointer to vector structure
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_clear(dc_vec_t* vec);

/**
 * @brief Reserve capacity for vector
 * @param vec Pointer to vector structure
 * @param capacity Minimum capacity to reserve
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_reserve(dc_vec_t* vec, size_t capacity);

/**
 * @brief Shrink vector capacity to fit current length
 * @param vec Pointer to vector structure
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_shrink_to_fit(dc_vec_t* vec);

/**
 * @brief Push element to end of vector
 * @param vec Pointer to vector structure
 * @param element Pointer to element to push
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_push(dc_vec_t* vec, const void* element);

/**
 * @brief Pop element from end of vector
 * @param vec Pointer to vector structure
 * @param element Pointer to store popped element (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_pop(dc_vec_t* vec, void* element);

/**
 * @brief Insert element at index
 * @param vec Pointer to vector structure
 * @param index Index to insert at
 * @param element Pointer to element to insert
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_insert(dc_vec_t* vec, size_t index, const void* element);

/**
 * @brief Remove element at index
 * @param vec Pointer to vector structure
 * @param index Index to remove
 * @param element Pointer to store removed element (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_remove(dc_vec_t* vec, size_t index, void* element);

/**
 * @brief Remove element at index by swapping with last element (unordered)
 * @param vec Pointer to vector structure
 * @param index Index to remove
 * @param element Pointer to store removed element (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_swap_remove(dc_vec_t* vec, size_t index, void* element);

/**
 * @brief Get element at index (bounds-checked)
 * @param vec Pointer to vector structure
 * @param index Index to get
 * @param element Pointer to store element
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_get(const dc_vec_t* vec, size_t index, void* element);

/**
 * @brief Set element at index (bounds-checked)
 * @param vec Pointer to vector structure
 * @param index Index to set
 * @param element Pointer to element to set
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_set(dc_vec_t* vec, size_t index, const void* element);

/**
 * @brief Get pointer to element at index (bounds-checked)
 * @param vec Pointer to vector structure
 * @param index Index to get
 * @return Pointer to element or NULL on error
 * 
 * @note The returned pointer is valid until the vector is modified
 */
void* dc_vec_at(const dc_vec_t* vec, size_t index);

/**
 * @brief Get pointer to first element
 * @param vec Pointer to vector structure
 * @return Pointer to first element or NULL if empty
 */
void* dc_vec_front(const dc_vec_t* vec);

/**
 * @brief Get pointer to last element
 * @param vec Pointer to vector structure
 * @return Pointer to last element or NULL if empty
 */
void* dc_vec_back(const dc_vec_t* vec);

/**
 * @brief Get raw data pointer
 * @param vec Pointer to vector structure
 * @return Raw data pointer or NULL if empty
 * 
 * @note The returned pointer is valid until the vector is modified
 */
void* dc_vec_data(const dc_vec_t* vec);

/**
 * @brief Get vector length
 * @param vec Pointer to vector structure
 * @return Number of elements or 0 if vector is invalid
 */
size_t dc_vec_length(const dc_vec_t* vec);

/**
 * @brief Get vector capacity
 * @param vec Pointer to vector structure
 * @return Capacity or 0 if vector is invalid
 */
size_t dc_vec_capacity(const dc_vec_t* vec);

/**
 * @brief Check if vector is empty
 * @param vec Pointer to vector structure
 * @return 1 if empty, 0 otherwise
 */
int dc_vec_is_empty(const dc_vec_t* vec);

/**
 * @brief Resize vector to new length
 * @param vec Pointer to vector structure
 * @param new_length New length
 * @return DC_OK on success, error code on failure
 * 
 * @note If new_length > current length, new elements are zero-initialized
 * @note If new_length < current length, excess elements are removed
 */
dc_status_t dc_vec_resize(dc_vec_t* vec, size_t new_length);

/**
 * @brief Find element in vector
 * @param vec Pointer to vector structure
 * @param element Pointer to element to find
 * @param compare Comparison function (NULL for memcmp)
 * @param index Pointer to store found index (optional)
 * @return DC_OK if found, DC_ERROR_NOT_FOUND if not found, other error codes on failure
 */
dc_status_t dc_vec_find(const dc_vec_t* vec, const void* element, 
                        int (*compare)(const void*, const void*), size_t* index);

/**
 * @brief Append multiple elements to vector
 * @param vec Pointer to vector structure
 * @param elements Pointer to elements to append
 * @param count Number of elements to append
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_append(dc_vec_t* vec, const void* elements, size_t count);

/**
 * @brief Copy vector contents into destination (deep copy of buffer)
 * @param dst Destination vector (will be reinitialized)
 * @param src Source vector
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_vec_copy(dc_vec_t* dst, const dc_vec_t* src);

/**
 * @brief Swap two vectors
 * @param a First vector
 * @param b Second vector
 */
void dc_vec_swap(dc_vec_t* a, dc_vec_t* b);

#ifdef __cplusplus
}
#endif

#endif /* DC_VEC_H */
