#ifndef DC_JSON_H
#define DC_JSON_H

/**
 * @file dc_json.h
 * @brief JSON parsing and serialization using yyjson
 */

#include <stddef.h>
#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_optional.h"
#include "core/dc_allowed_mentions.h"
#include "core/dc_attachments.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for yyjson types */
typedef struct yyjson_doc yyjson_doc;
typedef struct yyjson_val yyjson_val;
typedef struct yyjson_mut_doc yyjson_mut_doc;
typedef struct yyjson_mut_val yyjson_mut_val;

/**
 * @brief JSON document wrapper
 */
typedef struct {
    yyjson_doc* doc;        /**< yyjson document */
    yyjson_val* root;       /**< Root value */
} dc_json_doc_t;

/**
 * @brief JSON mutable document wrapper
 */
typedef struct {
    yyjson_mut_doc* doc;    /**< yyjson mutable document */
    yyjson_mut_val* root;   /**< Root value */
} dc_json_mut_doc_t;

/* Named optional/nullable helper types for JSON helpers */
typedef DC_OPTIONAL(const char*) dc_optional_cstr_t;
typedef DC_NULLABLE(const char*) dc_nullable_cstr_t;
typedef DC_OPTIONAL(int64_t) dc_optional_i64_t;
typedef DC_NULLABLE(int64_t) dc_nullable_i64_t;
typedef DC_OPTIONAL(uint64_t) dc_optional_u64_t;
typedef DC_NULLABLE(uint64_t) dc_nullable_u64_t;
typedef DC_OPTIONAL(int) dc_optional_int_t;
typedef DC_NULLABLE(int) dc_nullable_int_t;
typedef DC_OPTIONAL(double) dc_optional_double_t;
typedef DC_NULLABLE(double) dc_nullable_double_t;

/**
 * @brief Parse JSON from string
 * @param json_str JSON string
 * @param doc Pointer to store parsed document
 * @return DC_OK on success, error code on failure
 *
 * @note Caller must free any prior document in @p doc before reuse.
 */
dc_status_t dc_json_parse(const char* json_str, dc_json_doc_t* doc);

/**
 * @brief Parse JSON from string (relaxed: allow comments/trailing commas)
 * @param json_str JSON string
 * @param doc Pointer to store parsed document
 * @return DC_OK on success, error code on failure
 *
 * @note Caller must free any prior document in @p doc before reuse.
 */
dc_status_t dc_json_parse_relaxed(const char* json_str, dc_json_doc_t* doc);

/**
 * @brief Parse JSON from buffer
 * @param json_data JSON data buffer
 * @param json_len Length of JSON data
 * @param doc Pointer to store parsed document
 * @return DC_OK on success, error code on failure
 *
 * @note Caller must free any prior document in @p doc before reuse.
 */
dc_status_t dc_json_parse_buffer(const char* json_data, size_t json_len, dc_json_doc_t* doc);

/**
 * @brief Parse JSON from buffer (relaxed: allow comments/trailing commas)
 * @param json_data JSON data buffer
 * @param json_len Length of JSON data
 * @param doc Pointer to store parsed document
 * @return DC_OK on success, error code on failure
 *
 * @note Caller must free any prior document in @p doc before reuse.
 */
dc_status_t dc_json_parse_buffer_relaxed(const char* json_data, size_t json_len, dc_json_doc_t* doc);

/**
 * @brief Free JSON document
 * @param doc Document to free
 */
void dc_json_doc_free(dc_json_doc_t* doc);

/**
 * @brief Create mutable JSON document
 * @param doc Pointer to store created document
 * @return DC_OK on success, error code on failure
 *
 * @note Caller must free any prior document in @p doc before reuse.
 */
dc_status_t dc_json_mut_doc_create(dc_json_mut_doc_t* doc);

/**
 * @brief Free mutable JSON document
 * @param doc Document to free
 */
void dc_json_mut_doc_free(dc_json_mut_doc_t* doc);

/**
 * @brief Serialize mutable document to string
 * @param doc Document to serialize
 * @param result String to store result
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_json_mut_doc_serialize(const dc_json_mut_doc_t* doc, dc_string_t* result);

/* Value access helpers */
dc_status_t dc_json_get_string(yyjson_val* val, const char* key, const char** result);
dc_status_t dc_json_get_int64(yyjson_val* val, const char* key, int64_t* result);
dc_status_t dc_json_get_uint64(yyjson_val* val, const char* key, uint64_t* result);
dc_status_t dc_json_get_bool(yyjson_val* val, const char* key, int* result);
dc_status_t dc_json_get_double(yyjson_val* val, const char* key, double* result);
dc_status_t dc_json_get_object(yyjson_val* val, const char* key, yyjson_val** result);
dc_status_t dc_json_get_array(yyjson_val* val, const char* key, yyjson_val** result);

/* Optional value access helpers */
dc_status_t dc_json_get_string_opt(yyjson_val* val, const char* key, const char** result, const char* default_val);
dc_status_t dc_json_get_int64_opt(yyjson_val* val, const char* key, int64_t* result, int64_t default_val);
dc_status_t dc_json_get_uint64_opt(yyjson_val* val, const char* key, uint64_t* result, uint64_t default_val);
dc_status_t dc_json_get_bool_opt(yyjson_val* val, const char* key, int* result, int default_val);
dc_status_t dc_json_get_double_opt(yyjson_val* val, const char* key, double* result, double default_val);
dc_status_t dc_json_get_object_opt(yyjson_val* val, const char* key, yyjson_val** result);
dc_status_t dc_json_get_array_opt(yyjson_val* val, const char* key, yyjson_val** result);

/* Optional/nullable helpers (missing vs null are distinct) */
dc_status_t dc_json_get_string_optional(yyjson_val* val, const char* key, dc_optional_cstr_t* out);
dc_status_t dc_json_get_string_nullable(yyjson_val* val, const char* key, dc_nullable_cstr_t* out);
dc_status_t dc_json_get_int64_optional(yyjson_val* val, const char* key, dc_optional_i64_t* out);
dc_status_t dc_json_get_int64_nullable(yyjson_val* val, const char* key, dc_nullable_i64_t* out);
dc_status_t dc_json_get_uint64_optional(yyjson_val* val, const char* key, dc_optional_u64_t* out);
dc_status_t dc_json_get_uint64_nullable(yyjson_val* val, const char* key, dc_nullable_u64_t* out);
dc_status_t dc_json_get_bool_optional(yyjson_val* val, const char* key, dc_optional_int_t* out);
dc_status_t dc_json_get_bool_nullable(yyjson_val* val, const char* key, dc_nullable_int_t* out);
dc_status_t dc_json_get_double_optional(yyjson_val* val, const char* key, dc_optional_double_t* out);
dc_status_t dc_json_get_double_nullable(yyjson_val* val, const char* key, dc_nullable_double_t* out);

/* Snowflake helpers (Discord IDs are strings in JSON) */
dc_status_t dc_json_get_snowflake(yyjson_val* val, const char* key, uint64_t* result);
dc_status_t dc_json_get_snowflake_opt(yyjson_val* val, const char* key, uint64_t* result, uint64_t default_val);
dc_status_t dc_json_get_snowflake_optional(yyjson_val* val, const char* key, dc_optional_u64_t* out);
dc_status_t dc_json_get_snowflake_nullable(yyjson_val* val, const char* key, dc_nullable_u64_t* out);

/* Permission helpers (Discord permission bitfields are strings in JSON) */
dc_status_t dc_json_get_permission(yyjson_val* val, const char* key, uint64_t* result);
dc_status_t dc_json_get_permission_opt(yyjson_val* val, const char* key, uint64_t* result, uint64_t default_val);

/* Mutable value builders */
yyjson_mut_val* dc_json_mut_create_object(dc_json_mut_doc_t* doc);
yyjson_mut_val* dc_json_mut_create_array(dc_json_mut_doc_t* doc);
dc_status_t dc_json_mut_set_string(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key, const char* val);
dc_status_t dc_json_mut_set_int64(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key, int64_t val);
dc_status_t dc_json_mut_set_uint64(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key, uint64_t val);
dc_status_t dc_json_mut_set_bool(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key, int val);
dc_status_t dc_json_mut_set_null(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key);

/* Snowflake mutable helpers (Discord IDs are strings in JSON) */
dc_status_t dc_json_mut_set_snowflake(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key, uint64_t val);
dc_status_t dc_json_mut_set_permission(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const char* key, uint64_t val);

dc_status_t dc_json_mut_add_allowed_mentions(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                             const char* key,
                                             const dc_allowed_mentions_t* mentions);

dc_status_t dc_json_mut_add_attachments(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                        const char* key,
                                        const dc_attachment_descriptor_t* attachments,
                                        size_t count);

#ifdef __cplusplus
}
#endif

#endif /* DC_JSON_H */
