/**
 * @file dc_message.c
 * @brief Message model implementation
 */

#include "dc_message.h"
#include "json/dc_json.h"
#include "json/dc_json_model.h"
#include <string.h>

dc_status_t dc_message_init(dc_message_t* message) {
    if (!message) return DC_ERROR_NULL_POINTER;
    memset(message, 0, sizeof(*message));
    dc_status_t st = dc_user_init(&message->author);
    if (st != DC_OK) return st;
    st = dc_channel_init(&message->thread);
    if (st != DC_OK) return st;
    st = dc_string_init(&message->content);
    if (st != DC_OK) return st;
    st = dc_string_init(&message->timestamp);
    if (st != DC_OK) return st;
    st = dc_nullable_string_init(&message->edited_timestamp);
    if (st != DC_OK) return st;
    st = dc_vec_init(&message->mention_roles, sizeof(dc_snowflake_t));
    if (st != DC_OK) return st;
    st = dc_vec_init(&message->components, sizeof(dc_component_t));
    if (st != DC_OK) return st;
    return DC_OK;
}

void dc_message_free(dc_message_t* message) {
    if (!message) return;
    dc_user_free(&message->author);
    dc_channel_free(&message->thread);
    dc_string_free(&message->content);
    dc_string_free(&message->timestamp);
    dc_nullable_string_free(&message->edited_timestamp);
    dc_vec_free(&message->mention_roles);
    
    // Free all components
    for (size_t i = 0; i < dc_vec_length(&message->components); i++) {
        dc_component_t* component = (dc_component_t*)dc_vec_at(&message->components, i);
        dc_component_free(component);
    }
    dc_vec_free(&message->components);
    
    memset(message, 0, sizeof(*message));
}

dc_status_t dc_message_from_json(const char* json_data, dc_message_t* message) {
    if (!json_data || !message) return DC_ERROR_NULL_POINTER;
    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(json_data, &doc);
    if (st != DC_OK) return st;

    dc_message_t tmp;
    st = dc_message_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    st = dc_json_model_message_from_val(doc.root, &tmp);
    dc_json_doc_free(&doc);
    if (st != DC_OK) {
        dc_message_free(&tmp);
        return st;
    }
    *message = tmp;
    return DC_OK;
}

dc_status_t dc_message_to_json(const dc_message_t* message, dc_string_t* json_result) {
    if (!message || !json_result) return DC_ERROR_NULL_POINTER;
    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;

    st = dc_json_model_message_to_mut(&doc, doc.root, message);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }

    st = dc_json_mut_doc_serialize(&doc, json_result);
    dc_json_mut_doc_free(&doc);
    return st;
}
