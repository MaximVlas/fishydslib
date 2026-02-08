/**
 * @file dc_component.c
 * @brief Component model implementation
 */

#include "model/dc_component.h"
#include "core/dc_alloc.h"
#include <string.h>

dc_status_t dc_partial_emoji_init(dc_partial_emoji_t* emoji) {
    if (!emoji) return DC_ERROR_NULL_POINTER;
    memset(emoji, 0, sizeof(*emoji));

    dc_status_t st = dc_optional_string_init(&emoji->name);
    if (st != DC_OK) return st;

    dc_optional_snowflake_clear(&emoji->id);
    dc_optional_bool_clear(&emoji->animated);
    return DC_OK;
}

void dc_partial_emoji_free(dc_partial_emoji_t* emoji) {
    if (!emoji) return;
    dc_optional_string_free(&emoji->name);
    memset(emoji, 0, sizeof(*emoji));
}

dc_status_t dc_select_default_value_init(dc_select_default_value_t* default_val) {
    if (!default_val) return DC_ERROR_NULL_POINTER;
    memset(default_val, 0, sizeof(*default_val));
    return DC_OK;
}

void dc_select_default_value_free(dc_select_default_value_t* default_val) {
    if (!default_val) return;
    memset(default_val, 0, sizeof(*default_val));
}

dc_status_t dc_unfurled_media_item_init(dc_unfurled_media_item_t* media) {
    if (!media) return DC_ERROR_NULL_POINTER;
    memset(media, 0, sizeof(*media));

    dc_status_t st = dc_string_init(&media->url);
    if (st != DC_OK) return st;
    st = dc_optional_string_init(&media->proxy_url);
    if (st != DC_OK) return st;
    st = dc_optional_string_init(&media->content_type);
    if (st != DC_OK) return st;

    dc_optional_i32_clear(&media->height);
    dc_optional_i32_clear(&media->width);
    dc_optional_snowflake_clear(&media->attachment_id);
    return DC_OK;
}

void dc_unfurled_media_item_free(dc_unfurled_media_item_t* media) {
    if (!media) return;
    dc_string_free(&media->url);
    dc_optional_string_free(&media->proxy_url);
    dc_optional_string_free(&media->content_type);
    memset(media, 0, sizeof(*media));
}

dc_status_t dc_select_option_init(dc_select_option_t* option) {
    if (!option) return DC_ERROR_NULL_POINTER;
    memset(option, 0, sizeof(*option));

    dc_status_t st = dc_string_init(&option->label);
    if (st != DC_OK) return st;
    st = dc_string_init(&option->value);
    if (st != DC_OK) return st;
    st = dc_optional_string_init(&option->description);
    if (st != DC_OK) return st;

    option->emoji = NULL;
    dc_optional_bool_clear(&option->default_val);
    return DC_OK;
}

void dc_select_option_free(dc_select_option_t* option) {
    if (!option) return;

    dc_string_free(&option->label);
    dc_string_free(&option->value);
    dc_optional_string_free(&option->description);

    if (option->emoji) {
        dc_partial_emoji_free(option->emoji);
        dc_free(option->emoji);
        option->emoji = NULL;
    }

    memset(option, 0, sizeof(*option));
}

dc_status_t dc_media_gallery_item_init(dc_media_gallery_item_t* item) {
    if (!item) return DC_ERROR_NULL_POINTER;
    memset(item, 0, sizeof(*item));

    dc_status_t st = dc_unfurled_media_item_init(&item->media);
    if (st != DC_OK) return st;
    st = dc_optional_string_init(&item->description);
    if (st != DC_OK) return st;

    dc_optional_bool_clear(&item->spoiler);
    return DC_OK;
}

void dc_media_gallery_item_free(dc_media_gallery_item_t* item) {
    if (!item) return;
    dc_unfurled_media_item_free(&item->media);
    dc_optional_string_free(&item->description);
    memset(item, 0, sizeof(*item));
}

dc_status_t dc_component_init(dc_component_t* component) {
    if (!component) return DC_ERROR_NULL_POINTER;
    memset(component, 0, sizeof(*component));

    dc_status_t st = dc_optional_string_init(&component->custom_id);
    if (st != DC_OK) goto fail;
    st = dc_optional_string_init(&component->label);
    if (st != DC_OK) goto fail;
    st = dc_optional_string_init(&component->url);
    if (st != DC_OK) goto fail;
    st = dc_optional_string_init(&component->placeholder);
    if (st != DC_OK) goto fail;
    st = dc_optional_string_init(&component->value);
    if (st != DC_OK) goto fail;
    st = dc_optional_string_init(&component->description);
    if (st != DC_OK) goto fail;
    st = dc_optional_string_init(&component->content);
    if (st != DC_OK) goto fail;
    st = dc_optional_string_init(&component->name);
    if (st != DC_OK) goto fail;

    st = dc_vec_init(&component->options, sizeof(dc_select_option_t));
    if (st != DC_OK) goto fail;
    st = dc_vec_init(&component->default_values, sizeof(dc_select_default_value_t));
    if (st != DC_OK) goto fail;
    st = dc_vec_init(&component->channel_types, sizeof(int));
    if (st != DC_OK) goto fail;
    st = dc_vec_init(&component->components, sizeof(dc_component_t));
    if (st != DC_OK) goto fail;
    st = dc_vec_init(&component->items, sizeof(dc_media_gallery_item_t));
    if (st != DC_OK) goto fail;

    dc_optional_i32_clear(&component->id);
    dc_optional_i32_clear(&component->style);
    dc_optional_snowflake_clear(&component->sku_id);
    dc_optional_bool_clear(&component->disabled);
    dc_optional_i32_clear(&component->min_values);
    dc_optional_i32_clear(&component->max_values);
    dc_optional_bool_clear(&component->required);
    dc_optional_i32_clear(&component->min_length);
    dc_optional_i32_clear(&component->max_length);
    dc_optional_bool_clear(&component->spoiler);
    dc_optional_i32_clear(&component->accent_color);
    dc_optional_bool_clear(&component->divider);
    dc_optional_i32_clear(&component->spacing);
    component->spacing.value = 1;
    dc_optional_i32_clear(&component->size);

    component->emoji = NULL;
    component->accessory = NULL;
    component->media = NULL;
    component->component = NULL;
    component->file = NULL;
    return DC_OK;

fail:
    dc_component_free(component);
    return st;
}

void dc_component_free(dc_component_t* component) {
    size_t i = 0;
    if (!component) return;

    dc_optional_string_free(&component->custom_id);
    dc_optional_string_free(&component->label);
    dc_optional_string_free(&component->url);
    dc_optional_string_free(&component->placeholder);
    dc_optional_string_free(&component->value);
    dc_optional_string_free(&component->description);
    dc_optional_string_free(&component->content);
    dc_optional_string_free(&component->name);

    if (component->emoji) {
        dc_partial_emoji_free(component->emoji);
        dc_free(component->emoji);
        component->emoji = NULL;
    }

    for (i = 0; i < dc_vec_length(&component->options); i++) {
        dc_select_option_t* option = (dc_select_option_t*)dc_vec_at(&component->options, i);
        dc_select_option_free(option);
    }
    dc_vec_free(&component->options);

    for (i = 0; i < dc_vec_length(&component->default_values); i++) {
        dc_select_default_value_t* default_val =
            (dc_select_default_value_t*)dc_vec_at(&component->default_values, i);
        dc_select_default_value_free(default_val);
    }
    dc_vec_free(&component->default_values);

    dc_vec_free(&component->channel_types);

    for (i = 0; i < dc_vec_length(&component->components); i++) {
        dc_component_t* child_comp = (dc_component_t*)dc_vec_at(&component->components, i);
        dc_component_free(child_comp);
    }
    dc_vec_free(&component->components);

    if (component->accessory) {
        dc_component_free(component->accessory);
        dc_free(component->accessory);
        component->accessory = NULL;
    }
    if (component->component) {
        dc_component_free(component->component);
        dc_free(component->component);
        component->component = NULL;
    }
    if (component->media) {
        dc_unfurled_media_item_free(component->media);
        dc_free(component->media);
        component->media = NULL;
    }
    if (component->file) {
        dc_unfurled_media_item_free(component->file);
        dc_free(component->file);
        component->file = NULL;
    }

    for (i = 0; i < dc_vec_length(&component->items); i++) {
        dc_media_gallery_item_t* item = (dc_media_gallery_item_t*)dc_vec_at(&component->items, i);
        dc_media_gallery_item_free(item);
    }
    dc_vec_free(&component->items);

    memset(component, 0, sizeof(*component));
}
