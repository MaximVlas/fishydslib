#ifndef DC_COMPONENT_H
#define DC_COMPONENT_H

/**
 * @file dc_component.h
 * @brief Discord Component model
 */

#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_snowflake.h"
#include "core/dc_vec.h"
#include "model/dc_model_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Component types
typedef enum {
    DC_COMPONENT_TYPE_ACTION_ROW = 1,
    DC_COMPONENT_TYPE_BUTTON = 2,
    DC_COMPONENT_TYPE_STRING_SELECT = 3,
    DC_COMPONENT_TYPE_TEXT_INPUT = 4,
    DC_COMPONENT_TYPE_USER_SELECT = 5,
    DC_COMPONENT_TYPE_ROLE_SELECT = 6,
    DC_COMPONENT_TYPE_MENTIONABLE_SELECT = 7,
    DC_COMPONENT_TYPE_CHANNEL_SELECT = 8,
    DC_COMPONENT_TYPE_SECTION = 9,
    DC_COMPONENT_TYPE_TEXT_DISPLAY = 10,
    DC_COMPONENT_TYPE_THUMBNAIL = 11,
    DC_COMPONENT_TYPE_MEDIA_GALLERY = 12,
    DC_COMPONENT_TYPE_FILE = 13,
    DC_COMPONENT_TYPE_SEPARATOR = 14,
    DC_COMPONENT_TYPE_CONTAINER = 17,
    DC_COMPONENT_TYPE_LABEL = 18,
    DC_COMPONENT_TYPE_FILE_UPLOAD = 19
} dc_component_type_t;

// Button styles
typedef enum {
    DC_BUTTON_STYLE_PRIMARY = 1,
    DC_BUTTON_STYLE_SECONDARY = 2,
    DC_BUTTON_STYLE_SUCCESS = 3,
    DC_BUTTON_STYLE_DANGER = 4,
    DC_BUTTON_STYLE_LINK = 5,
    DC_BUTTON_STYLE_PREMIUM = 6
} dc_button_style_t;

// Text input styles
typedef enum {
    DC_TEXT_INPUT_STYLE_SHORT = 1,
    DC_TEXT_INPUT_STYLE_PARAGRAPH = 2
} dc_text_input_style_t;

// Select default value type
typedef enum {
    DC_SELECT_DEFAULT_VALUE_TYPE_USER = 0,
    DC_SELECT_DEFAULT_VALUE_TYPE_ROLE = 1,
    DC_SELECT_DEFAULT_VALUE_TYPE_CHANNEL = 2
} dc_select_default_value_type_t;

// Unfurled media item
typedef struct {
    dc_string_t url;
    dc_optional_string_t proxy_url;
    dc_optional_i32_t height;
    dc_optional_i32_t width;
    dc_optional_string_t content_type;
    dc_optional_snowflake_t attachment_id;
} dc_unfurled_media_item_t;

// Select default value
typedef struct {
    dc_snowflake_t id;
    dc_select_default_value_type_t type;
} dc_select_default_value_t;

// Emoji (partial)
typedef struct {
    dc_optional_snowflake_t id;
    dc_optional_string_t name;
    dc_optional_bool_t animated;
} dc_partial_emoji_t;

// Select option
typedef struct {
    dc_string_t label;
    dc_string_t value;
    dc_optional_string_t description;
    dc_partial_emoji_t* emoji;  /**< Heap-allocated via dc_calloc; freed by dc_select_option_free */
    dc_optional_bool_t default_val;
} dc_select_option_t;

// Media gallery item
typedef struct {
    dc_unfurled_media_item_t media;
    dc_optional_string_t description;
    dc_optional_bool_t spoiler;
} dc_media_gallery_item_t;

// Component structure
typedef struct dc_component dc_component_t;

struct dc_component {
    dc_component_type_t type;
    dc_optional_i32_t id;  // 32-bit integer identifier
    dc_optional_string_t custom_id;
    dc_optional_i32_t style;  // For buttons and text inputs
    dc_optional_string_t label;
    dc_partial_emoji_t* emoji;  /**< Heap-allocated via dc_calloc; freed by dc_component_free */
    dc_optional_string_t url;
    dc_optional_snowflake_t sku_id;
    dc_optional_bool_t disabled;
    dc_optional_string_t placeholder;
    dc_vec_t options;  // dc_select_option_t
    dc_vec_t default_values;  // dc_select_default_value_t
    dc_optional_i32_t min_values;
    dc_optional_i32_t max_values;
    dc_optional_bool_t required;
    dc_optional_i32_t min_length;
    dc_optional_i32_t max_length;
    dc_optional_string_t value;
    dc_vec_t channel_types;  // int (channel types)
    dc_vec_t components;  // dc_component_t (child components)
    dc_component_t* accessory;  /**< Heap-allocated; freed by dc_component_free */
    dc_unfurled_media_item_t* media;  /**< Heap-allocated; freed by dc_component_free */
    dc_optional_string_t description;
    dc_optional_bool_t spoiler;
    dc_optional_i32_t accent_color;
    dc_optional_bool_t divider;
    dc_optional_i32_t spacing;
    dc_component_t* component;  /**< Heap-allocated; freed by dc_component_free */
    dc_unfurled_media_item_t* file;  /**< Heap-allocated; freed by dc_component_free */
    dc_optional_string_t content;
    dc_optional_i32_t size;
    dc_optional_string_t name;
    dc_vec_t items;  // dc_media_gallery_item_t (for media galleries)
};

dc_status_t dc_component_init(dc_component_t* component);
void dc_component_free(dc_component_t* component);
dc_status_t dc_select_option_init(dc_select_option_t* option);
void dc_select_option_free(dc_select_option_t* option);
dc_status_t dc_unfurled_media_item_init(dc_unfurled_media_item_t* media);
void dc_unfurled_media_item_free(dc_unfurled_media_item_t* media);
dc_status_t dc_select_default_value_init(dc_select_default_value_t* default_val);
void dc_select_default_value_free(dc_select_default_value_t* default_val);
dc_status_t dc_partial_emoji_init(dc_partial_emoji_t* emoji);
void dc_partial_emoji_free(dc_partial_emoji_t* emoji);
dc_status_t dc_media_gallery_item_init(dc_media_gallery_item_t* item);
void dc_media_gallery_item_free(dc_media_gallery_item_t* item);

#ifdef __cplusplus
}
#endif

#endif /* DC_COMPONENT_H */
