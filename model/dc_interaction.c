#include "model/dc_interaction.h"
#include <string.h>

dc_status_t dc_interaction_data_init(dc_interaction_data_t* data) {
    if (!data) return DC_ERROR_NULL_POINTER;
    memset(data, 0, sizeof(*data));

    dc_status_t st = dc_string_init(&data->name);
    if (st != DC_OK) return st;
    st = dc_string_init(&data->custom_id);
    if (st != DC_OK) goto fail;
    st = dc_string_init(&data->options_json);
    if (st != DC_OK) goto fail;
    st = dc_string_init(&data->resolved_json);
    if (st != DC_OK) goto fail;
    st = dc_string_init(&data->values_json);
    if (st != DC_OK) goto fail;
    return DC_OK;

fail:
    dc_interaction_data_free(data);
    return st;
}

void dc_interaction_data_free(dc_interaction_data_t* data) {
    if (!data) return;
    dc_string_free(&data->name);
    dc_string_free(&data->custom_id);
    dc_string_free(&data->options_json);
    dc_string_free(&data->resolved_json);
    dc_string_free(&data->values_json);
    memset(data, 0, sizeof(*data));
}

dc_status_t dc_interaction_init(dc_interaction_t* interaction) {
    if (!interaction) return DC_ERROR_NULL_POINTER;
    memset(interaction, 0, sizeof(*interaction));

    dc_status_t st = dc_guild_member_init(&interaction->member);
    if (st != DC_OK) return st;
    st = dc_user_init(&interaction->user);
    if (st != DC_OK) goto fail;
    st = dc_string_init(&interaction->token);
    if (st != DC_OK) goto fail;
    st = dc_message_init(&interaction->message);
    if (st != DC_OK) goto fail;
    st = dc_optional_string_init(&interaction->app_permissions);
    if (st != DC_OK) goto fail;
    st = dc_optional_string_init(&interaction->locale);
    if (st != DC_OK) goto fail;
    st = dc_optional_string_init(&interaction->guild_locale);
    if (st != DC_OK) goto fail;
    st = dc_interaction_data_init(&interaction->data);
    if (st != DC_OK) goto fail;
    st = dc_string_init(&interaction->entitlements_json);
    if (st != DC_OK) goto fail;
    st = dc_string_init(&interaction->authorizing_integration_owners_json);
    if (st != DC_OK) goto fail;
    return DC_OK;

fail:
    dc_interaction_free(interaction);
    return st;
}

void dc_interaction_free(dc_interaction_t* interaction) {
    if (!interaction) return;
    dc_guild_member_free(&interaction->member);
    dc_user_free(&interaction->user);
    dc_string_free(&interaction->token);
    dc_message_free(&interaction->message);
    dc_optional_string_free(&interaction->app_permissions);
    dc_optional_string_free(&interaction->locale);
    dc_optional_string_free(&interaction->guild_locale);
    dc_interaction_data_free(&interaction->data);
    dc_string_free(&interaction->entitlements_json);
    dc_string_free(&interaction->authorizing_integration_owners_json);
    memset(interaction, 0, sizeof(*interaction));
}
