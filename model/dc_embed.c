#include "model/dc_embed.h"
#include <string.h>

void dc_embed_footer_free(dc_embed_footer_t* val) {
    dc_string_free(&val->text);
    dc_nullable_string_free(&val->icon_url);
    dc_nullable_string_free(&val->proxy_icon_url);
}

void dc_embed_image_free(dc_embed_image_t* val) {
    dc_string_free(&val->url);
    dc_nullable_string_free(&val->proxy_url);
}

void dc_embed_thumbnail_free(dc_embed_thumbnail_t* val) {
    dc_string_free(&val->url);
    dc_nullable_string_free(&val->proxy_url);
}

void dc_embed_video_free(dc_embed_video_t* val) {
    dc_nullable_string_free(&val->url);
    dc_nullable_string_free(&val->proxy_url);
}

void dc_embed_provider_free(dc_embed_provider_t* val) {
    dc_nullable_string_free(&val->name);
    dc_nullable_string_free(&val->url);
}

void dc_embed_author_free(dc_embed_author_t* val) {
    dc_string_free(&val->name);
    dc_nullable_string_free(&val->url);
    dc_nullable_string_free(&val->icon_url);
    dc_nullable_string_free(&val->proxy_icon_url);
}

void dc_embed_field_free(dc_embed_field_t* val) {
    dc_string_free(&val->name);
    dc_string_free(&val->value);
}

dc_status_t dc_embed_init(dc_embed_t* embed) {
    if (!embed) return DC_ERROR_NULL_POINTER;
    memset(embed, 0, sizeof(*embed));

    dc_status_t st = dc_nullable_string_init(&embed->title);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->type);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->description);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->url);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->timestamp);
    if (st != DC_OK) goto fail;

    /* Init sub-structs recursively */
    st = dc_string_init(&embed->footer.text);
    if(st!=DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->footer.icon_url);
    if(st!=DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->footer.proxy_icon_url);
    if(st!=DC_OK) goto fail;

    st = dc_string_init(&embed->image.url);
    if(st!=DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->image.proxy_url);
    if(st!=DC_OK) goto fail;

    st = dc_string_init(&embed->thumbnail.url);
    if(st!=DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->thumbnail.proxy_url);
    if(st!=DC_OK) goto fail;

    st = dc_nullable_string_init(&embed->video.url);
    if(st!=DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->video.proxy_url);
    if(st!=DC_OK) goto fail;

    st = dc_nullable_string_init(&embed->provider.name);
    if(st!=DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->provider.url);
    if(st!=DC_OK) goto fail;

    st = dc_string_init(&embed->author.name);
    if(st!=DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->author.url);
    if(st!=DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->author.icon_url);
    if(st!=DC_OK) goto fail;
    st = dc_nullable_string_init(&embed->author.proxy_icon_url);
    if(st!=DC_OK) goto fail;

    st = dc_vec_init(&embed->fields, sizeof(dc_embed_field_t));
    if (st != DC_OK) goto fail;

    return DC_OK;

fail:
    dc_embed_free(embed);
    return st;
}

void dc_embed_free(dc_embed_t* embed) {
    if (!embed) return;
    dc_nullable_string_free(&embed->title);
    dc_nullable_string_free(&embed->type);
    dc_nullable_string_free(&embed->description);
    dc_nullable_string_free(&embed->url);
    dc_nullable_string_free(&embed->timestamp);

    dc_embed_footer_free(&embed->footer);
    dc_embed_image_free(&embed->image);
    dc_embed_thumbnail_free(&embed->thumbnail);
    dc_embed_video_free(&embed->video);
    dc_embed_provider_free(&embed->provider);
    dc_embed_author_free(&embed->author);

    for (size_t i = 0; i < dc_vec_length(&embed->fields); i++) {
        dc_embed_field_free((dc_embed_field_t*)dc_vec_at(&embed->fields, i));
    }
    dc_vec_free(&embed->fields);

    memset(embed, 0, sizeof(*embed));
}
