#ifndef DC_ATTACHMENTS_H
#define DC_ATTACHMENTS_H

/**
 * @file dc_attachments.h
 * @brief Attachment helpers and validation
 */

#include <stddef.h>
#include <stdint.h>
#include "dc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Attachment descriptor for JSON payloads
 */
typedef struct {
    uint64_t id;            /**< Attachment id (index for uploads or existing id) */
    const char* filename;   /**< Optional filename */
    const char* description;/**< Optional description */
} dc_attachment_descriptor_t;

/**
 * @brief Validate attachment filename for embed usage
 * @param filename Filename to validate
 * @return 1 if valid, 0 otherwise
 */
int dc_attachment_filename_is_valid(const char* filename);

/**
 * @brief Validate attachment size against a limit
 * @param size Attachment size
 * @param max_size Max size (0 to skip)
 * @return 1 if valid, 0 otherwise
 */
int dc_attachment_size_is_valid(size_t size, size_t max_size);

/**
 * @brief Validate total attachment size against a limit
 * @param total Total size
 * @param max_total Max total size (0 to skip)
 * @return 1 if valid, 0 otherwise
 */
int dc_attachment_total_size_is_valid(size_t total, size_t max_total);

#ifdef __cplusplus
}
#endif

#endif /* DC_ATTACHMENTS_H */
