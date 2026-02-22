#ifndef DC_INTERACTION_H
#define DC_INTERACTION_H

/**
 * @file dc_interaction.h
 * @brief Discord interaction models for gateway INTERACTION_CREATE payloads
 */

#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_snowflake.h"
#include "model/dc_model_common.h"
#include "model/dc_user.h"
#include "model/dc_guild_member.h"
#include "model/dc_message.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Discord interaction types
 * @see https://discord.com/developers/docs/interactions/receiving-and-responding#interaction-object-interaction-type
 */
typedef enum {
    DC_INTERACTION_TYPE_PING = 1,
    DC_INTERACTION_TYPE_APPLICATION_COMMAND = 2,
    DC_INTERACTION_TYPE_MESSAGE_COMPONENT = 3,
    DC_INTERACTION_TYPE_APPLICATION_COMMAND_AUTOCOMPLETE = 4,
    DC_INTERACTION_TYPE_MODAL_SUBMIT = 5
} dc_interaction_type_t;

/**
 * @brief Application command option types
 * @see https://discord.com/developers/docs/interactions/application-commands#application-command-object-application-command-option-type
 */
typedef enum {
    DC_APPLICATION_COMMAND_OPTION_SUB_COMMAND = 1,
    DC_APPLICATION_COMMAND_OPTION_SUB_COMMAND_GROUP = 2,
    DC_APPLICATION_COMMAND_OPTION_STRING = 3,
    DC_APPLICATION_COMMAND_OPTION_INTEGER = 4,
    DC_APPLICATION_COMMAND_OPTION_BOOLEAN = 5,
    DC_APPLICATION_COMMAND_OPTION_USER = 6,
    DC_APPLICATION_COMMAND_OPTION_CHANNEL = 7,
    DC_APPLICATION_COMMAND_OPTION_ROLE = 8,
    DC_APPLICATION_COMMAND_OPTION_MENTIONABLE = 9,
    DC_APPLICATION_COMMAND_OPTION_NUMBER = 10,
    DC_APPLICATION_COMMAND_OPTION_ATTACHMENT = 11
} dc_application_command_option_type_t;

/**
 * @brief Interaction context types
 */
typedef enum {
    DC_INTERACTION_CONTEXT_GUILD = 0,
    DC_INTERACTION_CONTEXT_BOT_DM = 1,
    DC_INTERACTION_CONTEXT_PRIVATE_CHANNEL = 2
} dc_interaction_context_type_t;

/**
 * @brief Interaction data object (typed core fields + raw JSON payload sections)
 */
typedef struct {
    dc_optional_snowflake_t id;
    int has_name;
    dc_string_t name;
    int has_type;
    int type;
    dc_optional_snowflake_t target_id;
    dc_optional_snowflake_t guild_id;
    int has_custom_id;
    dc_string_t custom_id;
    int has_component_type;
    int component_type;
    int has_options;
    dc_string_t options_json;
    int has_resolved;
    dc_string_t resolved_json;
    int has_values;
    dc_string_t values_json;
} dc_interaction_data_t;

dc_status_t dc_interaction_data_init(dc_interaction_data_t* data);
void dc_interaction_data_free(dc_interaction_data_t* data);

/**
 * @brief Interaction object for gateway INTERACTION_CREATE payloads
 */
typedef struct {
    dc_snowflake_t id;
    dc_snowflake_t application_id;
    dc_interaction_type_t type;
    dc_optional_snowflake_t guild_id;
    dc_optional_snowflake_t channel_id;
    int has_member;
    dc_guild_member_t member;
    int has_user;
    dc_user_t user;
    dc_string_t token;
    int version;
    int has_message;
    dc_message_t message;
    dc_optional_string_t app_permissions;
    dc_optional_string_t locale;
    dc_optional_string_t guild_locale;
    int has_context;
    dc_interaction_context_type_t context;
    int has_data;
    dc_interaction_data_t data;
    int has_entitlements;
    dc_string_t entitlements_json;
    int has_authorizing_integration_owners;
    dc_string_t authorizing_integration_owners_json;
} dc_interaction_t;

dc_status_t dc_interaction_init(dc_interaction_t* interaction);
void dc_interaction_free(dc_interaction_t* interaction);

#ifdef __cplusplus
}
#endif

#endif /* DC_INTERACTION_H */
