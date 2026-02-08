#ifndef DC_STRING_H
#define DC_STRING_H

/**
 * @file dc_string.h
 * @brief Safe string type with length, capacity, and append/format helpers
 */

#include <stddef.h>
#include <stdarg.h>
#include "dc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dynamic string type with length and capacity tracking
 */
typedef struct {
    char* data;        /**< String data (null-terminated) */
    size_t length;     /**< Current length (excluding null terminator) */
    size_t capacity;   /**< Total capacity (including null terminator) */
} dc_string_t;

/**
 * @brief Initialize an empty string
 * @param str Pointer to string structure
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_init(dc_string_t* str);

/**
 * @brief Initialize string with initial capacity
 * @param str Pointer to string structure
 * @param capacity Initial capacity
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_init_with_capacity(dc_string_t* str, size_t capacity);

/**
 * @brief Initialize string from C string
 * @param str Pointer to string structure
 * @param cstr C string to copy from
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_init_from_cstr(dc_string_t* str, const char* cstr);

/**
 * @brief Initialize string from buffer with length
 * @param str Pointer to string structure
 * @param data Buffer to copy from
 * @param length Length of data
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_init_from_buffer(dc_string_t* str, const char* data, size_t length);

/**
 * @brief Free string resources
 * @param str Pointer to string structure
 */
void dc_string_free(dc_string_t* str);

/**
 * @brief Clear string content (keep capacity)
 * @param str Pointer to string structure
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_clear(dc_string_t* str);

/**
 * @brief Reserve capacity for string
 * @param str Pointer to string structure
 * @param capacity Minimum capacity to reserve
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_reserve(dc_string_t* str, size_t capacity);

/**
 * @brief Shrink string capacity to fit current length
 * @param str Pointer to string structure
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_shrink_to_fit(dc_string_t* str);

/**
 * @brief Append C string to string
 * @param str Pointer to string structure
 * @param cstr C string to append
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_append_cstr(dc_string_t* str, const char* cstr);

/**
 * @brief Append buffer to string
 * @param str Pointer to string structure
 * @param data Buffer to append
 * @param length Length of data
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_append_buffer(dc_string_t* str, const char* data, size_t length);

/**
 * @brief Append single character to string
 * @param str Pointer to string structure
 * @param c Character to append
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_append_char(dc_string_t* str, char c);

/**
 * @brief Append another string to string
 * @param str Pointer to string structure
 * @param other String to append
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_append_string(dc_string_t* str, const dc_string_t* other);

/**
 * @brief Format and append to string (printf-style)
 * @param str Pointer to string structure
 * @param format Format string
 * @param ... Format arguments
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_append_printf(dc_string_t* str, const char* format, ...);

/**
 * @brief Format and append to string (vprintf-style)
 * @param str Pointer to string structure
 * @param format Format string
 * @param args Format arguments
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_append_vprintf(dc_string_t* str, const char* format, va_list args);

/**
 * @brief Set string content from C string
 * @param str Pointer to string structure
 * @param cstr C string to copy from
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_set_cstr(dc_string_t* str, const char* cstr);

/**
 * @brief Set string content from buffer
 * @param str Pointer to string structure
 * @param data Buffer to copy from
 * @param length Length of data
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_set_buffer(dc_string_t* str, const char* data, size_t length);

/**
 * @brief Format string content (printf-style)
 * @param str Pointer to string structure
 * @param format Format string
 * @param ... Format arguments
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_printf(dc_string_t* str, const char* format, ...);

/**
 * @brief Format string content (vprintf-style)
 * @param str Pointer to string structure
 * @param format Format string
 * @param args Format arguments
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_string_vprintf(dc_string_t* str, const char* format, va_list args);

/**
 * @brief Get C string pointer (null-terminated)
 * @param str Pointer to string structure
 * @return C string pointer (empty string if invalid or uninitialized)
 * 
 * @note The returned pointer is valid until the string is modified
 */
const char* dc_string_cstr(const dc_string_t* str);

/**
 * @brief Get string length
 * @param str Pointer to string structure
 * @return String length or 0 if string is invalid
 */
size_t dc_string_length(const dc_string_t* str);

/**
 * @brief Get string capacity
 * @param str Pointer to string structure
 * @return String capacity or 0 if string is invalid
 */
size_t dc_string_capacity(const dc_string_t* str);

/**
 * @brief Check if string is empty
 * @param str Pointer to string structure
 * @return 1 if empty, 0 otherwise
 */
int dc_string_is_empty(const dc_string_t* str);

/**
 * @brief Compare string with C string
 * @param str Pointer to string structure
 * @param cstr C string to compare with
 * @return 0 if equal, negative if str < cstr, positive if str > cstr
 */
int dc_string_compare_cstr(const dc_string_t* str, const char* cstr);

/**
 * @brief Compare two strings
 * @param str1 First string
 * @param str2 Second string
 * @return 0 if equal, negative if str1 < str2, positive if str1 > str2
 */
int dc_string_compare(const dc_string_t* str1, const dc_string_t* str2);

#ifdef __cplusplus
}
#endif

#endif /* DC_STRING_H */
