#ifndef DC_ATTACHMENT_H
#define DC_ATTACHMENT_H

/**
 * @file dc_attachment.h
 * @brief Discord Attachment model
 */

#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_snowflake.h"
#include "model/dc_model_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dc_snowflake_t id;
    dc_string_t filename;
    dc_nullable_string_t description;
    dc_nullable_string_t content_type;
    size_t size;
    dc_string_t url;
    dc_string_t proxy_url;
    dc_optional_snowflake_t height; /* Using snowflake for optional uint */
    dc_optional_snowflake_t width;
    int ephemeral;
} dc_attachment_t;

dc_status_t dc_attachment_init(dc_attachment_t* attachment);
void dc_attachment_free(dc_attachment_t* attachment);

#ifdef __cplusplus
}
#endif

#endif /* DC_ATTACHMENT_H */
