#ifndef DC_DATA_URI_H
#define DC_DATA_URI_H

/**
 * @file dc_data_uri.h
 * @brief Data URI helpers for image payloads
 */

#include "dc_status.h"
#include "dc_string.h"
#include "dc_cdn.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validate a data URI for base64-encoded images
 * @param data_uri Data URI string
 * @return 1 if valid, 0 otherwise
 */
int dc_data_uri_is_valid_image_base64(const char* data_uri);

/**
 * @brief Build a data URI for a base64-encoded image
 * @param format Image format (png/jpg/gif/webp/avif)
 * @param base64 Base64 payload (no prefix)
 * @param out Output data URI
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_data_uri_build_image_base64(dc_cdn_image_format_t format,
                                           const char* base64,
                                           dc_string_t* out);

#ifdef __cplusplus
}
#endif

#endif /* DC_DATA_URI_H */
