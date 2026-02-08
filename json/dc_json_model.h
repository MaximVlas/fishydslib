#ifndef DC_JSON_MODEL_H
#define DC_JSON_MODEL_H

/**
 * @file dc_json_model.h
 * @brief JSON model parsing/building helpers
 */

#include "json/dc_json.h"
#include "model/dc_user.h"
#include "model/dc_guild_member.h"
#include "model/dc_role.h"
#include "model/dc_channel.h"
#include "model/dc_message.h"

#ifdef __cplusplus
extern "C" {
#endif

dc_status_t dc_json_model_user_from_val(yyjson_val* val, dc_user_t* user);
dc_status_t dc_json_model_guild_member_from_val(yyjson_val* val, dc_guild_member_t* member);
dc_status_t dc_json_model_role_from_val(yyjson_val* val, dc_role_t* role);
dc_status_t dc_json_model_channel_from_val(yyjson_val* val, dc_channel_t* channel);
dc_status_t dc_json_model_message_from_val(yyjson_val* val, dc_message_t* message);
dc_status_t dc_json_model_component_from_val(yyjson_val* val, dc_component_t* component);
dc_status_t dc_json_model_thread_member_from_val(yyjson_val* val, dc_channel_thread_member_t* member);

dc_status_t dc_json_model_user_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const dc_user_t* user);
dc_status_t dc_json_model_guild_member_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj,
                                              const dc_guild_member_t* member);
dc_status_t dc_json_model_role_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const dc_role_t* role);
dc_status_t dc_json_model_channel_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const dc_channel_t* channel);
dc_status_t dc_json_model_message_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const dc_message_t* message);
dc_status_t dc_json_model_component_to_mut(dc_json_mut_doc_t* doc, yyjson_mut_val* obj, const dc_component_t* component);

#ifdef __cplusplus
}
#endif

#endif /* DC_JSON_MODEL_H */
