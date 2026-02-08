#ifndef DC_EMBED_H
#define DC_EMBED_H

/**
 * @file dc_embed.h
 * @brief Discord Embed model
 */

#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_vec.h"
#include "model/dc_model_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dc_string_t text;
    dc_nullable_string_t icon_url;
    dc_nullable_string_t proxy_icon_url;
} dc_embed_footer_t;

typedef struct {
    dc_string_t url;
    dc_nullable_string_t proxy_url;
    int height;
    int width;
} dc_embed_image_t;

typedef struct {
    dc_string_t url;
    dc_nullable_string_t proxy_url;
    int height;
    int width;
} dc_embed_thumbnail_t;

typedef struct {
    dc_nullable_string_t url;
    dc_nullable_string_t proxy_url;
    int height;
    int width;
} dc_embed_video_t;

typedef struct {
    dc_nullable_string_t name;
    dc_nullable_string_t url;
} dc_embed_provider_t;

typedef struct {
    dc_string_t name;
    dc_nullable_string_t url;
    dc_nullable_string_t icon_url;
    dc_nullable_string_t proxy_icon_url;
} dc_embed_author_t;

typedef struct {
    dc_string_t name;
    dc_string_t value;
    int _inline; /* 'inline' is keyword in C */
} dc_embed_field_t;

typedef struct {
    dc_nullable_string_t title;
    dc_nullable_string_t type;
    dc_nullable_string_t description;
    dc_nullable_string_t url;
    dc_nullable_string_t timestamp;
    int color; /* 0 if not set? or separate flag? */
    
    int has_footer;
    dc_embed_footer_t footer;
    
    int has_image;
    dc_embed_image_t image;
    
    int has_thumbnail;
    dc_embed_thumbnail_t thumbnail;
    
    int has_video;
    dc_embed_video_t video;
    
    int has_provider;
    dc_embed_provider_t provider;
    
    int has_author;
    dc_embed_author_t author;
    
    dc_vec_t fields; /* dc_embed_field_t */
} dc_embed_t;

dc_status_t dc_embed_init(dc_embed_t* embed);
void dc_embed_free(dc_embed_t* embed);

/* Sub-struct inits/frees if needed, usually called by main init/free */
void dc_embed_footer_free(dc_embed_footer_t* val);
void dc_embed_image_free(dc_embed_image_t* val);
void dc_embed_thumbnail_free(dc_embed_thumbnail_t* val);
void dc_embed_video_free(dc_embed_video_t* val);
void dc_embed_provider_free(dc_embed_provider_t* val);
void dc_embed_author_free(dc_embed_author_t* val);
void dc_embed_field_free(dc_embed_field_t* val);

#ifdef __cplusplus
}
#endif

#endif /* DC_EMBED_H */
