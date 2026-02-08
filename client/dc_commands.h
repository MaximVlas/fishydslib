#ifndef DC_COMMANDS_H
#define DC_COMMANDS_H

/**
 * @file dc_commands.h
 * @brief Simple command router for message-based bots
 */

#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_vec.h"
#include "model/dc_message.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dc_client dc_client_t;

typedef dc_status_t (*dc_command_handler_t)(dc_client_t* client,
                                            const dc_message_t* message,
                                            const char* args,
                                            void* user_data);

typedef struct {
    const char* name;                 /**< Command name (without prefix) */
    const char* help;                 /**< Optional help text */
    dc_command_handler_t handler;     /**< Handler function */
} dc_command_t;

typedef struct {
    dc_string_t prefix;               /**< Command prefix (e.g. "!") */
    dc_vec_t commands;                /**< Command list (dc_command_t) */
    void* user_data;                  /**< Passed to handlers */
    int ignore_bots;                  /**< Ignore bot authors */
    int case_insensitive;             /**< Match commands case-insensitively */
} dc_command_router_t;

dc_status_t dc_command_router_init(dc_command_router_t* router, const char* prefix);
void dc_command_router_free(dc_command_router_t* router);
dc_status_t dc_command_router_add(dc_command_router_t* router, const dc_command_t* command);
dc_status_t dc_command_router_add_many(dc_command_router_t* router,
                                       const dc_command_t* commands,
                                       size_t count);
dc_status_t dc_command_router_set_prefix(dc_command_router_t* router, const char* prefix);
void dc_command_router_set_ignore_bots(dc_command_router_t* router, int ignore);
void dc_command_router_set_case_insensitive(dc_command_router_t* router, int enable);

dc_status_t dc_command_router_handle_message(dc_command_router_t* router,
                                             dc_client_t* client,
                                             const dc_message_t* message);
dc_status_t dc_command_router_handle_event(dc_command_router_t* router,
                                           dc_client_t* client,
                                           const char* event_name,
                                           const char* event_json);

#ifdef __cplusplus
}
#endif

#endif /* DC_COMMANDS_H */
