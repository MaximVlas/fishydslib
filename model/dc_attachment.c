#include "model/dc_attachment.h"
#include "model/dc_model_common.h"
#include <string.h>

dc_status_t dc_attachment_init(dc_attachment_t* attachment) {
    if (!attachment) return DC_ERROR_NULL_POINTER;
    memset(attachment, 0, sizeof(*attachment));
    
    dc_status_t st = dc_string_init(&attachment->filename);
    if (st != DC_OK) return st;
    
    st = dc_string_init(&attachment->url);
    if (st != DC_OK) {
        dc_string_free(&attachment->filename);
        return st;
    }
    
    st = dc_string_init(&attachment->proxy_url);
    if (st != DC_OK) {
        dc_string_free(&attachment->filename);
        dc_string_free(&attachment->url);
        return st;
    }

    st = dc_nullable_string_init(&attachment->description);
    if (st != DC_OK) goto fail;

    st = dc_nullable_string_init(&attachment->content_type);
    if (st != DC_OK) goto fail;

    return DC_OK;

fail:
    dc_attachment_free(attachment);
    return st;
}

void dc_attachment_free(dc_attachment_t* attachment) {
    if (!attachment) return;
    dc_string_free(&attachment->filename);
    dc_nullable_string_free(&attachment->description);
    dc_nullable_string_free(&attachment->content_type);
    dc_string_free(&attachment->url);
    dc_string_free(&attachment->proxy_url);
    memset(attachment, 0, sizeof(*attachment));
}
