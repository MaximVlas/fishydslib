/**
 * @file dc_role.c
 * @brief Role model implementation
 */

#include "dc_role.h"
#include "json/dc_json.h"
#include "json/dc_json_model.h"
#include <string.h>

dc_status_t dc_role_init(dc_role_t* role) {
    if (!role) return DC_ERROR_NULL_POINTER;
    memset(role, 0, sizeof(*role));

    dc_status_t st = dc_string_init(&role->name);
    if (st != DC_OK) return st;
    st = dc_nullable_string_init(&role->icon);
    if (st != DC_OK) goto fail;
    st = dc_nullable_string_init(&role->unicode_emoji);
    if (st != DC_OK) goto fail;

    return DC_OK;

fail:
    dc_role_free(role);
    return st;
}

void dc_role_free(dc_role_t* role) {
    if (!role) return;
    dc_string_free(&role->name);
    dc_nullable_string_free(&role->icon);
    dc_nullable_string_free(&role->unicode_emoji);
    memset(role, 0, sizeof(*role));
}

dc_status_t dc_role_from_json(const char* json_data, dc_role_t* role) {
    if (!json_data || !role) return DC_ERROR_NULL_POINTER;

    dc_json_doc_t doc;
    dc_status_t st = dc_json_parse(json_data, &doc);
    if (st != DC_OK) return st;

    dc_role_t tmp;
    st = dc_role_init(&tmp);
    if (st != DC_OK) {
        dc_json_doc_free(&doc);
        return st;
    }

    st = dc_json_model_role_from_val(doc.root, &tmp);
    dc_json_doc_free(&doc);
    if (st != DC_OK) {
        dc_role_free(&tmp);
        return st;
    }

    *role = tmp;
    return DC_OK;
}

dc_status_t dc_role_to_json(const dc_role_t* role, dc_string_t* json_result) {
    if (!role || !json_result) return DC_ERROR_NULL_POINTER;

    dc_json_mut_doc_t doc;
    dc_status_t st = dc_json_mut_doc_create(&doc);
    if (st != DC_OK) return st;

    st = dc_json_model_role_to_mut(&doc, doc.root, role);
    if (st != DC_OK) {
        dc_json_mut_doc_free(&doc);
        return st;
    }

    st = dc_json_mut_doc_serialize(&doc, json_result);
    dc_json_mut_doc_free(&doc);
    return st;
}

dc_status_t dc_role_list_init(dc_role_list_t* list) {
    if (!list) return DC_ERROR_NULL_POINTER;
    memset(list, 0, sizeof(*list));
    return dc_vec_init(&list->items, sizeof(dc_role_t));
}

void dc_role_list_free(dc_role_list_t* list) {
    if (!list) return;
    for (size_t i = 0; i < dc_vec_length(&list->items); i++) {
        dc_role_t* role = (dc_role_t*)dc_vec_at(&list->items, i);
        if (role) dc_role_free(role);
    }
    dc_vec_free(&list->items);
    memset(list, 0, sizeof(*list));
}
