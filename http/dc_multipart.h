#ifndef DC_MULTIPART_H
#define DC_MULTIPART_H

/**
 * @file dc_multipart.h
 * @brief Multipart/form-data builder
 */

#include <stddef.h>
#include "core/dc_status.h"
#include "core/dc_string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dc_string_t boundary;     /**< Boundary token (no leading --) */
    dc_string_t body;         /**< Multipart body data */
    size_t part_count;        /**< Number of parts added */
    size_t file_count;        /**< Number of files added */
    size_t max_file_size;     /**< Max file size (0 = unlimited) */
    size_t max_total_size;    /**< Max total size (0 = unlimited) */
    size_t total_size;        /**< Total size so far */
    int finalized;            /**< Final boundary appended */
} dc_multipart_t;

/**
 * @brief Initialize multipart builder
 */
dc_status_t dc_multipart_init(dc_multipart_t* mp);

/**
 * @brief Free multipart builder
 */
void dc_multipart_free(dc_multipart_t* mp);

/**
 * @brief Set explicit boundary
 */
dc_status_t dc_multipart_set_boundary(dc_multipart_t* mp, const char* boundary);

/**
 * @brief Set size limits (0 to disable)
 */
dc_status_t dc_multipart_set_limits(dc_multipart_t* mp, size_t max_file_size, size_t max_total_size);

/**
 * @brief Build Content-Type header value
 */
dc_status_t dc_multipart_get_content_type(const dc_multipart_t* mp, dc_string_t* out);

/**
 * @brief Add a simple field
 */
dc_status_t dc_multipart_add_field(dc_multipart_t* mp, const char* name, const char* value);

/**
 * @brief Add payload_json field
 */
dc_status_t dc_multipart_add_payload_json(dc_multipart_t* mp, const char* json);

/**
 * @brief Add a file part (auto files[n])
 */
dc_status_t dc_multipart_add_file(dc_multipart_t* mp, const char* filename,
                                  const void* data, size_t size,
                                  const char* content_type,
                                  size_t* out_index);

/**
 * @brief Add a file part with explicit field name
 */
dc_status_t dc_multipart_add_file_named(dc_multipart_t* mp, const char* field_name,
                                        const char* filename,
                                        const void* data, size_t size,
                                        const char* content_type);

/**
 * @brief Finalize multipart body (append closing boundary)
 */
dc_status_t dc_multipart_finish(dc_multipart_t* mp);

#ifdef __cplusplus
}
#endif

#endif /* DC_MULTIPART_H */
