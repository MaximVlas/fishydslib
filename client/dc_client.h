#ifndef DC_CLIENT_H
#define DC_CLIENT_H

/**
 * @file dc_client.h
 * @brief Main Discord client API
 */

#include <stddef.h>
#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_log.h"
#include "core/dc_string.h"
#include "core/dc_snowflake.h"
#include "http/dc_http_compliance.h"
#include "gw/dc_gateway.h"
#include "model/dc_user.h"
#include "model/dc_guild.h"
#include "model/dc_guild_member.h"
#include "model/dc_role.h"
#include "model/dc_channel.h"
#include "model/dc_message.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Discord client structure
 */
typedef struct dc_client dc_client_t;

/**
 * @brief Gateway bot info (from /gateway/bot)
 */
typedef struct {
    dc_string_t url;                   /**< Gateway URL */
    uint32_t shards;                   /**< Recommended shard count */
    uint32_t session_limit_total;      /**< Session start limit total */
    uint32_t session_limit_remaining;  /**< Session start limit remaining */
    uint32_t session_limit_reset_after_ms; /**< Session start reset in ms */
    uint32_t session_limit_max_concurrency; /**< Max concurrency */
} dc_gateway_info_t;

/**
 * @brief Initialize gateway info
 */
dc_status_t dc_gateway_info_init(dc_gateway_info_t* info);

/**
 * @brief Free gateway info
 */
void dc_gateway_info_free(dc_gateway_info_t* info);

/**
 * @brief Client configuration
 */
typedef struct {
    const char* token;                          /**< Bot token */
    dc_http_auth_type_t auth_type;              /**< Bot/Bearer */
    uint32_t intents;                           /**< Gateway intents */
    uint32_t shard_id;                          /**< Shard id (optional, requires shard_count) */
    uint32_t shard_count;                       /**< Total shards (optional) */
    uint32_t large_threshold;                   /**< Identify large_threshold (50-250, 0 to omit) */
    const char* user_agent;                     /**< User agent string */
    dc_user_agent_t user_agent_info;            /**< User agent descriptor */
    int use_user_agent_info;                    /**< Use user_agent_info when user_agent is not set */
    dc_gateway_event_callback_t event_callback; /**< Event callback */
    dc_gateway_state_callback_t state_callback; /**< State callback */
    void* user_data;                            /**< User data for callbacks */
    uint32_t http_timeout_ms;                   /**< HTTP request timeout */
    uint32_t gateway_timeout_ms;                /**< Gateway timeout */
    int enable_compression;                     /**< Enable compression */
    int enable_payload_compression;             /**< Enable Identify payload compression */
    dc_log_callback_t log_callback;             /**< Optional log callback */
    void* log_user_data;                        /**< User data for log callback */
    dc_log_level_t log_level;                   /**< Log level filter */
} dc_client_config_t;

/**
 * @brief Initialize client configuration with defaults
 *
 * Defaults:
 * - auth_type: Bot
 * - http_timeout_ms: 30000
 * - gateway_timeout_ms: 60000
 * - log_level: INFO
 */
void dc_client_config_init(dc_client_config_t* config);

/**
 * @brief Set user agent info (formatted internally)
 * @param config Client configuration
 * @param ua User-Agent descriptor
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_config_set_user_agent_info(dc_client_config_t* config, const dc_user_agent_t* ua);

/**
 * @brief Create Discord client
 * @param config Client configuration
 * @param client Pointer to store created client
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_create(const dc_client_config_t* config, dc_client_t** client);

/**
 * @brief Initialize client (alias of dc_client_create)
 */
dc_status_t dc_client_init(const dc_client_config_t* config, dc_client_t** client);

/**
 * @brief Free Discord client
 * @param client Client to free
 */
void dc_client_free(dc_client_t* client);

/**
 * @brief Shutdown client (alias of dc_client_free)
 */
void dc_client_shutdown(dc_client_t* client);

/**
 * @brief Set logger callback on an existing client
 * @param client Discord client
 * @param callback Log callback (NULL to disable)
 * @param user_data User data for callback
 * @param level Log level filter
 */
void dc_client_set_logger(dc_client_t* client, dc_log_callback_t callback, void* user_data, dc_log_level_t level);

/**
 * @brief Start client (connect to gateway)
 * @param client Discord client
 * @return DC_OK on success, error code on failure
 *
 * @note Not thread-safe; call from a single thread.
 */
dc_status_t dc_client_start(dc_client_t* client);

/**
 * @brief Start client with explicit gateway URL (skips REST /gateway/bot)
 * @param client Discord client
 * @param gateway_url Gateway URL (e.g., wss://gateway.discord.gg/?v=10&encoding=json)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_start_with_gateway_url(dc_client_t* client, const char* gateway_url);

/**
 * @brief Stop client (disconnect from gateway)
 * @param client Discord client
 * @return DC_OK on success, error code on failure
 *
 * @note Not thread-safe; call from a single thread.
 */
dc_status_t dc_client_stop(dc_client_t* client);

/**
 * @brief Process client events (call regularly in event loop)
 * @param client Discord client
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking)
 * @return DC_OK on success, error code on failure
 *
 * @note Callbacks run on the calling thread; do not block in callbacks.
 */
dc_status_t dc_client_process(dc_client_t* client, uint32_t timeout_ms);

/**
 * @brief Get gateway info from REST /gateway/bot
 * @param client Discord client
 * @param info Gateway info output (overwritten)
 * @return DC_OK on success, error code on failure
 *
 * @note Output overwrites @p info without freeing prior contents.
 */
dc_status_t dc_client_get_gateway_info(dc_client_t* client, dc_gateway_info_t* info);

/**
 * @brief Get current user
 * @param client Discord client
 * @param user User to store result
 * @return DC_OK on success, error code on failure
 *
 * @note Output overwrites @p user without freeing prior contents.
 */
dc_status_t dc_client_get_current_user(dc_client_t* client, dc_user_t* user);

/**
 * @brief Get user by ID
 * @param client Discord client
 * @param user_id User ID
 * @param user User to store result
 * @return DC_OK on success, error code on failure
 *
 * @note Output overwrites @p user without freeing prior contents.
 */
dc_status_t dc_client_get_user(dc_client_t* client, dc_snowflake_t user_id, dc_user_t* user);

/**
 * @brief Create message in channel
 * @param client Discord client
 * @param channel_id Channel ID
 * @param content Message content
 * @param message_id Pointer to store created message ID (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_create_message(dc_client_t* client, dc_snowflake_t channel_id, 
                                      const char* content, dc_snowflake_t* message_id);

/**
 * @brief Create message in channel with a raw JSON payload
 * @param client Discord client
 * @param channel_id Channel ID
 * @param json_body Message JSON payload
 * @param message_id Pointer to store created message ID (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_create_message_json(dc_client_t* client,
                                          dc_snowflake_t channel_id,
                                          const char* json_body,
                                          dc_snowflake_t* message_id);

/**
 * @brief Get guild JSON object
 * @param client Discord client
 * @param guild_id Guild ID
 * @param guild_json Output JSON string (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_get_guild_json(dc_client_t* client,
                                     dc_snowflake_t guild_id,
                                     dc_string_t* guild_json);

/**
 * @brief Get guild by ID as typed model
 * @param client Discord client
 * @param guild_id Guild ID
 * @param guild Output guild (overwritten)
 * @return DC_OK on success, error code on failure
 *
 * @note Output overwrites @p guild without freeing prior contents.
 */
dc_status_t dc_client_get_guild(dc_client_t* client,
                                dc_snowflake_t guild_id,
                                dc_guild_t* guild);

/**
 * @brief Get guild channels JSON array
 * @param client Discord client
 * @param guild_id Guild ID
 * @param channels_json Output JSON string (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_get_guild_channels_json(dc_client_t* client,
                                              dc_snowflake_t guild_id,
                                              dc_string_t* channels_json);

/**
 * @brief Get guild channels as typed list
 * @param client Discord client
 * @param guild_id Guild ID
 * @param channels Output channel list (overwritten)
 * @return DC_OK on success, error code on failure
 *
 * @note Output overwrites @p channels without freeing prior contents.
 */
dc_status_t dc_client_get_guild_channels(dc_client_t* client,
                                         dc_snowflake_t guild_id,
                                         dc_channel_list_t* channels);

/**
 * @brief Get channel by ID
 * @param client Discord client
 * @param channel_id Channel ID
 * @param channel Output channel (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_get_channel(dc_client_t* client,
                                  dc_snowflake_t channel_id,
                                  dc_channel_t* channel);

/**
 * @brief Modify channel using JSON patch body
 * @param client Discord client
 * @param channel_id Channel ID
 * @param json_body JSON payload for PATCH /channels/{channel.id}
 * @param channel Output channel (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_modify_channel_json(dc_client_t* client,
                                          dc_snowflake_t channel_id,
                                          const char* json_body,
                                          dc_channel_t* channel);

/**
 * @brief Delete/close channel
 * @param client Discord client
 * @param channel_id Channel ID
 * @param channel Output deleted channel (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_delete_channel(dc_client_t* client,
                                     dc_snowflake_t channel_id,
                                     dc_channel_t* channel);

/**
 * @brief List channel messages as JSON array
 * @param client Discord client
 * @param channel_id Channel ID
 * @param limit Max messages (1-100, 0 to use default)
 * @param before Message ID cursor (0 to omit)
 * @param after Message ID cursor (0 to omit)
 * @param around Message ID cursor (0 to omit)
 * @param messages_json Output JSON string (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_list_channel_messages_json(dc_client_t* client,
                                                 dc_snowflake_t channel_id,
                                                 uint32_t limit,
                                                 dc_snowflake_t before,
                                                 dc_snowflake_t after,
                                                 dc_snowflake_t around,
                                                 dc_string_t* messages_json);

/**
 * @brief Get message by ID
 * @param client Discord client
 * @param channel_id Channel ID
 * @param message_id Message ID
 * @param message Output message (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_get_message(dc_client_t* client,
                                  dc_snowflake_t channel_id,
                                  dc_snowflake_t message_id,
                                  dc_message_t* message);

/**
 * @brief Edit message content
 * @param client Discord client
 * @param channel_id Channel ID
 * @param message_id Message ID
 * @param content Updated content
 * @param message Output updated message (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_edit_message_content(dc_client_t* client,
                                           dc_snowflake_t channel_id,
                                           dc_snowflake_t message_id,
                                           const char* content,
                                           dc_message_t* message);

/**
 * @brief Delete message
 * @param client Discord client
 * @param channel_id Channel ID
 * @param message_id Message ID
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_delete_message(dc_client_t* client,
                                     dc_snowflake_t channel_id,
                                     dc_snowflake_t message_id);

/**
 * @brief Pin message in channel
 * @param client Discord client
 * @param channel_id Channel ID
 * @param message_id Message ID
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_pin_message(dc_client_t* client,
                                  dc_snowflake_t channel_id,
                                  dc_snowflake_t message_id);

/**
 * @brief Unpin message in channel
 * @param client Discord client
 * @param channel_id Channel ID
 * @param message_id Message ID
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_unpin_message(dc_client_t* client,
                                    dc_snowflake_t channel_id,
                                    dc_snowflake_t message_id);

/**
 * @brief List pinned messages as JSON array
 * @param client Discord client
 * @param channel_id Channel ID
 * @param messages_json Output JSON string (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_get_pinned_messages_json(dc_client_t* client,
                                               dc_snowflake_t channel_id,
                                               dc_string_t* messages_json);

/**
 * @brief Modify guild using JSON patch body
 * @param client Discord client
 * @param guild_id Guild ID
 * @param json_body JSON payload for PATCH /guilds/{guild.id}
 * @param guild_json Output guild JSON (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_modify_guild_json(dc_client_t* client,
                                        dc_snowflake_t guild_id,
                                        const char* json_body,
                                        dc_string_t* guild_json);

/**
 * @brief Modify guild using JSON patch body and parse typed guild response
 * @param client Discord client
 * @param guild_id Guild ID
 * @param json_body JSON payload for PATCH /guilds/{guild.id}
 * @param guild Output guild (optional, overwritten when non-NULL)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_modify_guild(dc_client_t* client,
                                   dc_snowflake_t guild_id,
                                   const char* json_body,
                                   dc_guild_t* guild);

/**
 * @brief Create guild channel using JSON body
 * @param client Discord client
 * @param guild_id Guild ID
 * @param json_body JSON payload for POST /guilds/{guild.id}/channels
 * @param channel Output channel (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_create_guild_channel_json(dc_client_t* client,
                                                dc_snowflake_t guild_id,
                                                const char* json_body,
                                                dc_channel_t* channel);

/**
 * @brief Get guild member JSON
 * @param client Discord client
 * @param guild_id Guild ID
 * @param user_id User ID
 * @param member_json Output member JSON (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_get_guild_member_json(dc_client_t* client,
                                            dc_snowflake_t guild_id,
                                            dc_snowflake_t user_id,
                                            dc_string_t* member_json);

/**
 * @brief Get guild member as typed model
 * @param client Discord client
 * @param guild_id Guild ID
 * @param user_id User ID
 * @param member Output guild member (overwritten)
 * @return DC_OK on success, error code on failure
 *
 * @note Output overwrites @p member without freeing prior contents.
 */
dc_status_t dc_client_get_guild_member(dc_client_t* client,
                                       dc_snowflake_t guild_id,
                                       dc_snowflake_t user_id,
                                       dc_guild_member_t* member);

/**
 * @brief List guild members JSON
 * @param client Discord client
 * @param guild_id Guild ID
 * @param limit Max members (1-1000, 0 uses default)
 * @param after Cursor user ID (0 to omit)
 * @param members_json Output members JSON (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_list_guild_members_json(dc_client_t* client,
                                              dc_snowflake_t guild_id,
                                              uint32_t limit,
                                              dc_snowflake_t after,
                                              dc_string_t* members_json);

/**
 * @brief List guild members as typed models
 * @param client Discord client
 * @param guild_id Guild ID
 * @param limit Max members (1-1000, 0 uses default)
 * @param after Cursor user ID (0 to omit)
 * @param members Output member list (overwritten)
 * @return DC_OK on success, error code on failure
 *
 * @note Output overwrites @p members without freeing prior contents.
 */
dc_status_t dc_client_list_guild_members(dc_client_t* client,
                                         dc_snowflake_t guild_id,
                                         uint32_t limit,
                                         dc_snowflake_t after,
                                         dc_guild_member_list_t* members);

/**
 * @brief Get guild roles JSON
 * @param client Discord client
 * @param guild_id Guild ID
 * @param roles_json Output roles JSON (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_get_guild_roles_json(dc_client_t* client,
                                           dc_snowflake_t guild_id,
                                           dc_string_t* roles_json);

/**
 * @brief Get guild roles as typed models
 * @param client Discord client
 * @param guild_id Guild ID
 * @param roles Output role list (overwritten)
 * @return DC_OK on success, error code on failure
 *
 * @note Output overwrites @p roles without freeing prior contents.
 */
dc_status_t dc_client_get_guild_roles(dc_client_t* client,
                                      dc_snowflake_t guild_id,
                                      dc_role_list_t* roles);

/**
 * @brief Create guild role using JSON body
 * @param client Discord client
 * @param guild_id Guild ID
 * @param json_body JSON payload for POST /guilds/{guild.id}/roles
 * @param role Output role (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_create_guild_role_json(dc_client_t* client,
                                             dc_snowflake_t guild_id,
                                             const char* json_body,
                                             dc_role_t* role);

/**
 * @brief Modify guild role positions using JSON body
 * @param client Discord client
 * @param guild_id Guild ID
 * @param json_body JSON payload for PATCH /guilds/{guild.id}/roles
 * @param roles Output role list (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_modify_guild_role_positions_json(dc_client_t* client,
                                                       dc_snowflake_t guild_id,
                                                       const char* json_body,
                                                       dc_role_list_t* roles);

/**
 * @brief Modify guild role using JSON body
 * @param client Discord client
 * @param guild_id Guild ID
 * @param role_id Role ID
 * @param json_body JSON payload for PATCH /guilds/{guild.id}/roles/{role.id}
 * @param role Output role (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_modify_guild_role_json(dc_client_t* client,
                                             dc_snowflake_t guild_id,
                                             dc_snowflake_t role_id,
                                             const char* json_body,
                                             dc_role_t* role);

/**
 * @brief Delete guild role
 * @param client Discord client
 * @param guild_id Guild ID
 * @param role_id Role ID
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_delete_guild_role(dc_client_t* client,
                                        dc_snowflake_t guild_id,
                                        dc_snowflake_t role_id);

/**
 * @brief Remove guild member (kick)
 * @param client Discord client
 * @param guild_id Guild ID
 * @param user_id User ID
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_remove_guild_member(dc_client_t* client,
                                          dc_snowflake_t guild_id,
                                          dc_snowflake_t user_id);

/**
 * @brief Add role to guild member
 * @param client Discord client
 * @param guild_id Guild ID
 * @param user_id User ID
 * @param role_id Role ID
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_add_guild_member_role(dc_client_t* client,
                                            dc_snowflake_t guild_id,
                                            dc_snowflake_t user_id,
                                            dc_snowflake_t role_id);

/**
 * @brief Remove role from guild member
 * @param client Discord client
 * @param guild_id Guild ID
 * @param user_id User ID
 * @param role_id Role ID
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_remove_guild_member_role(dc_client_t* client,
                                               dc_snowflake_t guild_id,
                                               dc_snowflake_t user_id,
                                               dc_snowflake_t role_id);

/**
 * @brief Create channel webhook using JSON body
 * @param client Discord client
 * @param channel_id Channel ID
 * @param json_body JSON payload for POST /channels/{channel.id}/webhooks
 * @param webhook_json Output webhook JSON (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_create_channel_webhook_json(dc_client_t* client,
                                                  dc_snowflake_t channel_id,
                                                  const char* json_body,
                                                  dc_string_t* webhook_json);

/**
 * @brief Get channel webhooks JSON
 * @param client Discord client
 * @param channel_id Channel ID
 * @param webhooks_json Output webhooks JSON (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_get_channel_webhooks_json(dc_client_t* client,
                                                dc_snowflake_t channel_id,
                                                dc_string_t* webhooks_json);

/**
 * @brief Get guild webhooks JSON
 * @param client Discord client
 * @param guild_id Guild ID
 * @param webhooks_json Output webhooks JSON (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_get_guild_webhooks_json(dc_client_t* client,
                                              dc_snowflake_t guild_id,
                                              dc_string_t* webhooks_json);

/**
 * @brief Get webhook JSON
 * @param client Discord client
 * @param webhook_id Webhook ID
 * @param webhook_json Output webhook JSON (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_get_webhook_json(dc_client_t* client,
                                       dc_snowflake_t webhook_id,
                                       dc_string_t* webhook_json);

/**
 * @brief Get webhook with token JSON
 * @param client Discord client
 * @param webhook_id Webhook ID
 * @param webhook_token Webhook token
 * @param webhook_json Output webhook JSON (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_get_webhook_with_token_json(dc_client_t* client,
                                                  dc_snowflake_t webhook_id,
                                                  const char* webhook_token,
                                                  dc_string_t* webhook_json);

/**
 * @brief Modify webhook JSON
 * @param client Discord client
 * @param webhook_id Webhook ID
 * @param json_body JSON patch body
 * @param webhook_json Output webhook JSON (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_modify_webhook_json(dc_client_t* client,
                                          dc_snowflake_t webhook_id,
                                          const char* json_body,
                                          dc_string_t* webhook_json);

/**
 * @brief Modify webhook with token JSON
 * @param client Discord client
 * @param webhook_id Webhook ID
 * @param webhook_token Webhook token
 * @param json_body JSON patch body
 * @param webhook_json Output webhook JSON (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_modify_webhook_with_token_json(dc_client_t* client,
                                                     dc_snowflake_t webhook_id,
                                                     const char* webhook_token,
                                                     const char* json_body,
                                                     dc_string_t* webhook_json);

/**
 * @brief Delete webhook
 * @param client Discord client
 * @param webhook_id Webhook ID
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_delete_webhook(dc_client_t* client,
                                     dc_snowflake_t webhook_id);

/**
 * @brief Delete webhook with token
 * @param client Discord client
 * @param webhook_id Webhook ID
 * @param webhook_token Webhook token
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_delete_webhook_with_token(dc_client_t* client,
                                                dc_snowflake_t webhook_id,
                                                const char* webhook_token);

/**
 * @brief Execute webhook using JSON body
 * @param client Discord client
 * @param webhook_id Webhook ID
 * @param webhook_token Webhook token
 * @param json_body JSON execute body
 * @param wait Non-zero to wait for created message response
 * @param message_json Output message JSON when wait=true (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_execute_webhook_json(dc_client_t* client,
                                           dc_snowflake_t webhook_id,
                                           const char* webhook_token,
                                           const char* json_body,
                                           int wait,
                                           dc_string_t* message_json);

/**
 * @brief Get webhook message JSON
 * @param client Discord client
 * @param webhook_id Webhook ID
 * @param webhook_token Webhook token
 * @param message_id Message ID
 * @param thread_id Thread ID (0 to omit)
 * @param message_json Output message JSON (overwritten)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_get_webhook_message_json(dc_client_t* client,
                                               dc_snowflake_t webhook_id,
                                               const char* webhook_token,
                                               dc_snowflake_t message_id,
                                               dc_snowflake_t thread_id,
                                               dc_string_t* message_json);

/**
 * @brief Edit webhook message JSON
 * @param client Discord client
 * @param webhook_id Webhook ID
 * @param webhook_token Webhook token
 * @param message_id Message ID
 * @param json_body JSON patch body
 * @param thread_id Thread ID (0 to omit)
 * @param message_json Output message JSON (optional)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_edit_webhook_message_json(dc_client_t* client,
                                                dc_snowflake_t webhook_id,
                                                const char* webhook_token,
                                                dc_snowflake_t message_id,
                                                const char* json_body,
                                                dc_snowflake_t thread_id,
                                                dc_string_t* message_json);

/**
 * @brief Delete webhook message
 * @param client Discord client
 * @param webhook_id Webhook ID
 * @param webhook_token Webhook token
 * @param message_id Message ID
 * @param thread_id Thread ID (0 to omit)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_delete_webhook_message(dc_client_t* client,
                                             dc_snowflake_t webhook_id,
                                             const char* webhook_token,
                                             dc_snowflake_t message_id,
                                             dc_snowflake_t thread_id);

/* Additional REST endpoints (JSON-oriented wrappers) */
dc_status_t dc_client_get_current_application_json(dc_client_t* client,
                                                   dc_string_t* application_json);
dc_status_t dc_client_modify_current_application_json(dc_client_t* client,
                                                      const char* json_body,
                                                      dc_string_t* application_json);
dc_status_t dc_client_get_application_role_connection_metadata_json(dc_client_t* client,
                                                                    dc_snowflake_t application_id,
                                                                    dc_string_t* metadata_json);
dc_status_t dc_client_update_application_role_connection_metadata_json(dc_client_t* client,
                                                                       dc_snowflake_t application_id,
                                                                       const char* json_body,
                                                                       dc_string_t* metadata_json);
dc_status_t dc_client_get_global_application_commands_json(dc_client_t* client,
                                                           dc_snowflake_t application_id,
                                                           int with_localizations,
                                                           dc_string_t* commands_json);
dc_status_t dc_client_create_global_application_command_json(dc_client_t* client,
                                                             dc_snowflake_t application_id,
                                                             const char* json_body,
                                                             dc_string_t* command_json);
dc_status_t dc_client_get_global_application_command_json(dc_client_t* client,
                                                          dc_snowflake_t application_id,
                                                          dc_snowflake_t command_id,
                                                          dc_string_t* command_json);
dc_status_t dc_client_modify_global_application_command_json(dc_client_t* client,
                                                             dc_snowflake_t application_id,
                                                             dc_snowflake_t command_id,
                                                             const char* json_body,
                                                             dc_string_t* command_json);
dc_status_t dc_client_delete_global_application_command(dc_client_t* client,
                                                        dc_snowflake_t application_id,
                                                        dc_snowflake_t command_id);
dc_status_t dc_client_bulk_overwrite_global_application_commands_json(dc_client_t* client,
                                                                      dc_snowflake_t application_id,
                                                                      const char* json_body,
                                                                      dc_string_t* commands_json);
dc_status_t dc_client_get_guild_application_commands_json(dc_client_t* client,
                                                          dc_snowflake_t application_id,
                                                          dc_snowflake_t guild_id,
                                                          int with_localizations,
                                                          dc_string_t* commands_json);
dc_status_t dc_client_create_guild_application_command_json(dc_client_t* client,
                                                            dc_snowflake_t application_id,
                                                            dc_snowflake_t guild_id,
                                                            const char* json_body,
                                                            dc_string_t* command_json);
dc_status_t dc_client_get_guild_application_command_json(dc_client_t* client,
                                                         dc_snowflake_t application_id,
                                                         dc_snowflake_t guild_id,
                                                         dc_snowflake_t command_id,
                                                         dc_string_t* command_json);
dc_status_t dc_client_modify_guild_application_command_json(dc_client_t* client,
                                                            dc_snowflake_t application_id,
                                                            dc_snowflake_t guild_id,
                                                            dc_snowflake_t command_id,
                                                            const char* json_body,
                                                            dc_string_t* command_json);
dc_status_t dc_client_delete_guild_application_command(dc_client_t* client,
                                                       dc_snowflake_t application_id,
                                                       dc_snowflake_t guild_id,
                                                       dc_snowflake_t command_id);
dc_status_t dc_client_bulk_overwrite_guild_application_commands_json(dc_client_t* client,
                                                                     dc_snowflake_t application_id,
                                                                     dc_snowflake_t guild_id,
                                                                     const char* json_body,
                                                                     dc_string_t* commands_json);
dc_status_t dc_client_get_guild_application_command_permissions_json(dc_client_t* client,
                                                                     dc_snowflake_t application_id,
                                                                     dc_snowflake_t guild_id,
                                                                     dc_string_t* permissions_json);
dc_status_t dc_client_get_application_command_permissions_json(dc_client_t* client,
                                                               dc_snowflake_t application_id,
                                                               dc_snowflake_t guild_id,
                                                               dc_snowflake_t command_id,
                                                               dc_string_t* permission_json);
dc_status_t dc_client_edit_application_command_permissions_json(dc_client_t* client,
                                                                dc_snowflake_t application_id,
                                                                dc_snowflake_t guild_id,
                                                                dc_snowflake_t command_id,
                                                                const char* json_body,
                                                                dc_string_t* permission_json);
dc_status_t dc_client_batch_edit_application_command_permissions_json(dc_client_t* client,
                                                                      dc_snowflake_t application_id,
                                                                      dc_snowflake_t guild_id,
                                                                      const char* json_body,
                                                                      dc_string_t* permissions_json);

dc_status_t dc_client_crosspost_message_json(dc_client_t* client,
                                             dc_snowflake_t channel_id,
                                             dc_snowflake_t message_id,
                                             dc_string_t* message_json);
dc_status_t dc_client_edit_message_json(dc_client_t* client,
                                        dc_snowflake_t channel_id,
                                        dc_snowflake_t message_id,
                                        const char* json_body,
                                        dc_message_t* message);
dc_status_t dc_client_bulk_delete_messages_json(dc_client_t* client,
                                                dc_snowflake_t channel_id,
                                                const char* json_body);
dc_status_t dc_client_create_reaction_encoded(dc_client_t* client,
                                              dc_snowflake_t channel_id,
                                              dc_snowflake_t message_id,
                                              const char* emoji_encoded);
dc_status_t dc_client_delete_own_reaction_encoded(dc_client_t* client,
                                                  dc_snowflake_t channel_id,
                                                  dc_snowflake_t message_id,
                                                  const char* emoji_encoded);
dc_status_t dc_client_delete_user_reaction_encoded(dc_client_t* client,
                                                   dc_snowflake_t channel_id,
                                                   dc_snowflake_t message_id,
                                                   const char* emoji_encoded,
                                                   dc_snowflake_t user_id);
dc_status_t dc_client_get_reactions_encoded_json(dc_client_t* client,
                                                 dc_snowflake_t channel_id,
                                                 dc_snowflake_t message_id,
                                                 const char* emoji_encoded,
                                                 int reaction_type,
                                                 dc_snowflake_t after,
                                                 uint32_t limit,
                                                 dc_string_t* users_json);
dc_status_t dc_client_delete_all_reactions(dc_client_t* client,
                                           dc_snowflake_t channel_id,
                                           dc_snowflake_t message_id);
dc_status_t dc_client_delete_all_reactions_for_emoji_encoded(dc_client_t* client,
                                                             dc_snowflake_t channel_id,
                                                             dc_snowflake_t message_id,
                                                             const char* emoji_encoded);
dc_status_t dc_client_get_channel_pins_json(dc_client_t* client,
                                            dc_snowflake_t channel_id,
                                            const char* before_iso8601,
                                            uint32_t limit,
                                            dc_string_t* pins_json);
dc_status_t dc_client_edit_channel_permissions_json(dc_client_t* client,
                                                    dc_snowflake_t channel_id,
                                                    dc_snowflake_t overwrite_id,
                                                    const char* json_body);
dc_status_t dc_client_delete_channel_permission(dc_client_t* client,
                                                dc_snowflake_t channel_id,
                                                dc_snowflake_t overwrite_id);
dc_status_t dc_client_get_channel_invites_json(dc_client_t* client,
                                               dc_snowflake_t channel_id,
                                               dc_string_t* invites_json);
dc_status_t dc_client_create_channel_invite_json(dc_client_t* client,
                                                 dc_snowflake_t channel_id,
                                                 const char* json_body,
                                                 dc_string_t* invite_json);
dc_status_t dc_client_get_invite_json(dc_client_t* client,
                                      const char* invite_code,
                                      int with_counts,
                                      int with_expiration,
                                      dc_snowflake_t guild_scheduled_event_id,
                                      dc_string_t* invite_json);
dc_status_t dc_client_delete_invite_json(dc_client_t* client,
                                         const char* invite_code,
                                         dc_string_t* invite_json);
dc_status_t dc_client_follow_announcement_channel_json(dc_client_t* client,
                                                       dc_snowflake_t channel_id,
                                                       const char* json_body,
                                                       dc_string_t* followed_channel_json);
dc_status_t dc_client_trigger_typing_indicator(dc_client_t* client,
                                               dc_snowflake_t channel_id);
dc_status_t dc_client_start_thread_from_message_json(dc_client_t* client,
                                                     dc_snowflake_t channel_id,
                                                     dc_snowflake_t message_id,
                                                     const char* json_body,
                                                     dc_string_t* thread_json);
dc_status_t dc_client_start_thread_without_message_json(dc_client_t* client,
                                                        dc_snowflake_t channel_id,
                                                        const char* json_body,
                                                        dc_string_t* thread_json);
dc_status_t dc_client_start_forum_or_media_thread_json(dc_client_t* client,
                                                       dc_snowflake_t channel_id,
                                                       const char* json_body,
                                                       dc_string_t* thread_json);
dc_status_t dc_client_join_thread(dc_client_t* client, dc_snowflake_t thread_id);
dc_status_t dc_client_add_thread_member(dc_client_t* client,
                                        dc_snowflake_t thread_id,
                                        dc_snowflake_t user_id);
dc_status_t dc_client_leave_thread(dc_client_t* client, dc_snowflake_t thread_id);
dc_status_t dc_client_remove_thread_member(dc_client_t* client,
                                           dc_snowflake_t thread_id,
                                           dc_snowflake_t user_id);
dc_status_t dc_client_get_thread_member_json(dc_client_t* client,
                                             dc_snowflake_t thread_id,
                                             dc_snowflake_t user_id,
                                             int with_member,
                                             dc_string_t* member_json);
dc_status_t dc_client_list_thread_members_json(dc_client_t* client,
                                               dc_snowflake_t thread_id,
                                               int with_member,
                                               dc_snowflake_t after,
                                               uint32_t limit,
                                               dc_string_t* members_json);
dc_status_t dc_client_list_public_archived_threads_json(dc_client_t* client,
                                                        dc_snowflake_t channel_id,
                                                        const char* before_iso8601,
                                                        uint32_t limit,
                                                        dc_string_t* threads_json);
dc_status_t dc_client_list_private_archived_threads_json(dc_client_t* client,
                                                         dc_snowflake_t channel_id,
                                                         const char* before_iso8601,
                                                         uint32_t limit,
                                                         dc_string_t* threads_json);
dc_status_t dc_client_list_joined_private_archived_threads_json(dc_client_t* client,
                                                                dc_snowflake_t channel_id,
                                                                dc_snowflake_t before,
                                                                uint32_t limit,
                                                                dc_string_t* threads_json);

dc_status_t dc_client_get_guild_preview_json(dc_client_t* client,
                                             dc_snowflake_t guild_id,
                                             dc_string_t* preview_json);
dc_status_t dc_client_modify_guild_channel_positions_json(dc_client_t* client,
                                                          dc_snowflake_t guild_id,
                                                          const char* json_body);
dc_status_t dc_client_list_active_guild_threads_json(dc_client_t* client,
                                                     dc_snowflake_t guild_id,
                                                     dc_string_t* threads_json);
dc_status_t dc_client_search_guild_members_json(dc_client_t* client,
                                                dc_snowflake_t guild_id,
                                                const char* query,
                                                uint32_t limit,
                                                dc_string_t* members_json);
dc_status_t dc_client_modify_guild_member_json(dc_client_t* client,
                                               dc_snowflake_t guild_id,
                                               dc_snowflake_t user_id,
                                               const char* json_body,
                                               dc_string_t* member_json);
dc_status_t dc_client_modify_current_member_json(dc_client_t* client,
                                                 dc_snowflake_t guild_id,
                                                 const char* json_body,
                                                 dc_string_t* member_json);
dc_status_t dc_client_modify_current_user_nick_json(dc_client_t* client,
                                                    dc_snowflake_t guild_id,
                                                    const char* json_body,
                                                    dc_string_t* nick_json);
dc_status_t dc_client_get_guild_bans_json(dc_client_t* client,
                                          dc_snowflake_t guild_id,
                                          uint32_t limit,
                                          dc_snowflake_t before,
                                          dc_snowflake_t after,
                                          dc_string_t* bans_json);
dc_status_t dc_client_get_guild_ban_json(dc_client_t* client,
                                         dc_snowflake_t guild_id,
                                         dc_snowflake_t user_id,
                                         dc_string_t* ban_json);
dc_status_t dc_client_create_guild_ban(dc_client_t* client,
                                       dc_snowflake_t guild_id,
                                       dc_snowflake_t user_id,
                                       int delete_message_seconds);
dc_status_t dc_client_remove_guild_ban(dc_client_t* client,
                                       dc_snowflake_t guild_id,
                                       dc_snowflake_t user_id);
dc_status_t dc_client_bulk_guild_ban_json(dc_client_t* client,
                                          dc_snowflake_t guild_id,
                                          const char* json_body,
                                          dc_string_t* result_json);
dc_status_t dc_client_get_guild_role_json(dc_client_t* client,
                                          dc_snowflake_t guild_id,
                                          dc_snowflake_t role_id,
                                          dc_string_t* role_json);
dc_status_t dc_client_get_guild_role_member_counts_json(dc_client_t* client,
                                                        dc_snowflake_t guild_id,
                                                        dc_string_t* counts_json);
dc_status_t dc_client_get_guild_prune_count_json(dc_client_t* client,
                                                 dc_snowflake_t guild_id,
                                                 uint32_t days,
                                                 const char* include_roles_csv,
                                                 dc_string_t* prune_json);
dc_status_t dc_client_begin_guild_prune_json(dc_client_t* client,
                                             dc_snowflake_t guild_id,
                                             const char* json_body,
                                             dc_string_t* prune_json);
dc_status_t dc_client_get_guild_voice_regions_json(dc_client_t* client,
                                                   dc_snowflake_t guild_id,
                                                   dc_string_t* regions_json);
dc_status_t dc_client_get_guild_invites_json(dc_client_t* client,
                                             dc_snowflake_t guild_id,
                                             dc_string_t* invites_json);
dc_status_t dc_client_get_guild_integrations_json(dc_client_t* client,
                                                  dc_snowflake_t guild_id,
                                                  dc_string_t* integrations_json);
dc_status_t dc_client_delete_guild_integration(dc_client_t* client,
                                               dc_snowflake_t guild_id,
                                               dc_snowflake_t integration_id);
dc_status_t dc_client_list_guild_scheduled_events_json(dc_client_t* client,
                                                       dc_snowflake_t guild_id,
                                                       int with_user_count,
                                                       dc_string_t* events_json);
dc_status_t dc_client_create_guild_scheduled_event_json(dc_client_t* client,
                                                        dc_snowflake_t guild_id,
                                                        const char* json_body,
                                                        dc_string_t* event_json);
dc_status_t dc_client_get_guild_scheduled_event_json(dc_client_t* client,
                                                     dc_snowflake_t guild_id,
                                                     dc_snowflake_t event_id,
                                                     int with_user_count,
                                                     dc_string_t* event_json);
dc_status_t dc_client_modify_guild_scheduled_event_json(dc_client_t* client,
                                                        dc_snowflake_t guild_id,
                                                        dc_snowflake_t event_id,
                                                        const char* json_body,
                                                        dc_string_t* event_json);
dc_status_t dc_client_delete_guild_scheduled_event(dc_client_t* client,
                                                   dc_snowflake_t guild_id,
                                                   dc_snowflake_t event_id);
dc_status_t dc_client_get_guild_scheduled_event_users_json(dc_client_t* client,
                                                           dc_snowflake_t guild_id,
                                                           dc_snowflake_t event_id,
                                                           uint32_t limit,
                                                           int with_member,
                                                           dc_snowflake_t before,
                                                           dc_snowflake_t after,
                                                           dc_string_t* users_json);
dc_status_t dc_client_list_guild_emojis_json(dc_client_t* client,
                                             dc_snowflake_t guild_id,
                                             dc_string_t* emojis_json);
dc_status_t dc_client_get_guild_emoji_json(dc_client_t* client,
                                           dc_snowflake_t guild_id,
                                           dc_snowflake_t emoji_id,
                                           dc_string_t* emoji_json);
dc_status_t dc_client_create_guild_emoji_json(dc_client_t* client,
                                              dc_snowflake_t guild_id,
                                              const char* json_body,
                                              dc_string_t* emoji_json);
dc_status_t dc_client_modify_guild_emoji_json(dc_client_t* client,
                                              dc_snowflake_t guild_id,
                                              dc_snowflake_t emoji_id,
                                              const char* json_body,
                                              dc_string_t* emoji_json);
dc_status_t dc_client_delete_guild_emoji(dc_client_t* client,
                                         dc_snowflake_t guild_id,
                                         dc_snowflake_t emoji_id);
dc_status_t dc_client_get_sticker_json(dc_client_t* client,
                                       dc_snowflake_t sticker_id,
                                       dc_string_t* sticker_json);
dc_status_t dc_client_list_sticker_packs_json(dc_client_t* client,
                                              dc_string_t* sticker_packs_json);
dc_status_t dc_client_get_sticker_pack_json(dc_client_t* client,
                                            dc_snowflake_t pack_id,
                                            dc_string_t* sticker_pack_json);
dc_status_t dc_client_list_guild_stickers_json(dc_client_t* client,
                                               dc_snowflake_t guild_id,
                                               dc_string_t* stickers_json);
dc_status_t dc_client_get_guild_sticker_json(dc_client_t* client,
                                             dc_snowflake_t guild_id,
                                             dc_snowflake_t sticker_id,
                                             dc_string_t* sticker_json);
dc_status_t dc_client_create_guild_sticker_multipart(dc_client_t* client,
                                                     dc_snowflake_t guild_id,
                                                     const char* name,
                                                     const char* description,
                                                     const char* tags,
                                                     const void* file_data,
                                                     size_t file_size,
                                                     const char* filename,
                                                     const char* content_type,
                                                     dc_string_t* sticker_json);
dc_status_t dc_client_modify_guild_sticker_json(dc_client_t* client,
                                                dc_snowflake_t guild_id,
                                                dc_snowflake_t sticker_id,
                                                const char* json_body,
                                                dc_string_t* sticker_json);
dc_status_t dc_client_delete_guild_sticker(dc_client_t* client,
                                           dc_snowflake_t guild_id,
                                           dc_snowflake_t sticker_id);

/**
 * @brief Update presence
 * @param client Discord client
 * @param status Status string ("online", "idle", "dnd", "invisible")
 * @param activity_name Activity name (optional)
 * @param activity_type Activity type (0=playing, 1=streaming, 2=listening, 3=watching, 5=competing)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_update_presence(dc_client_t* client, const char* status,
                                       const char* activity_name, int activity_type);

/**
 * @brief Create a simple application command (slash) with one string option
 * @param client Discord client
 * @param application_id Application ID
 * @param guild_id Guild ID for guild command (0 for global)
 * @param name Command name
 * @param description Command description
 * @param option_name Option name (string)
 * @param option_description Option description
 * @param option_required Non-zero if required
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_create_command_simple(dc_client_t* client,
                                            dc_snowflake_t application_id,
                                            dc_snowflake_t guild_id,
                                            const char* name,
                                            const char* description,
                                            const char* option_name,
                                            const char* option_description,
                                            int option_required);
dc_status_t dc_client_create_user_command_simple(dc_client_t* client,
                                                 dc_snowflake_t application_id,
                                                 dc_snowflake_t guild_id,
                                                 const char* name);
dc_status_t dc_client_create_message_command_simple(dc_client_t* client,
                                                    dc_snowflake_t application_id,
                                                    dc_snowflake_t guild_id,
                                                    const char* name);

/**
 * @brief Interaction callback types for POST /interactions/{id}/{token}/callback
 */
typedef enum {
    DC_INTERACTION_CALLBACK_PONG = 1,
    DC_INTERACTION_CALLBACK_CHANNEL_MESSAGE_WITH_SOURCE = 4,
    DC_INTERACTION_CALLBACK_DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE = 5,
    DC_INTERACTION_CALLBACK_DEFERRED_UPDATE_MESSAGE = 6,
    DC_INTERACTION_CALLBACK_UPDATE_MESSAGE = 7,
    DC_INTERACTION_CALLBACK_APPLICATION_COMMAND_AUTOCOMPLETE_RESULT = 8,
    DC_INTERACTION_CALLBACK_MODAL = 9,
    DC_INTERACTION_CALLBACK_LAUNCH_ACTIVITY = 12
} dc_interaction_callback_type_t;

/**
 * @brief Send a raw interaction callback with optional JSON data object
 * @param client Discord client
 * @param interaction_id Interaction ID
 * @param interaction_token Interaction token
 * @param callback_type Interaction callback type
 * @param data_json JSON object for "data" field (NULL to omit)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_callback_json(dc_client_t* client,
                                                dc_snowflake_t interaction_id,
                                                const char* interaction_token,
                                                dc_interaction_callback_type_t callback_type,
                                                const char* data_json);

/**
 * @brief Respond to an interaction with callback type 4 and raw data JSON
 * @param client Discord client
 * @param interaction_id Interaction ID
 * @param interaction_token Interaction token
 * @param data_json JSON object for callback "data"
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_respond_message_json(dc_client_t* client,
                                                       dc_snowflake_t interaction_id,
                                                       const char* interaction_token,
                                                       const char* data_json);

/**
 * @brief Defer an interaction with callback type 5
 * @param client Discord client
 * @param interaction_id Interaction ID
 * @param interaction_token Interaction token
 * @param ephemeral Non-zero to set ephemeral flag (64)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_defer_message(dc_client_t* client,
                                                dc_snowflake_t interaction_id,
                                                const char* interaction_token,
                                                int ephemeral);

/**
 * @brief Defer a message component interaction update (callback type 6)
 * @param client Discord client
 * @param interaction_id Interaction ID
 * @param interaction_token Interaction token
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_defer_update(dc_client_t* client,
                                               dc_snowflake_t interaction_id,
                                               const char* interaction_token);

/**
 * @brief Update the originating message for a component interaction (callback type 7)
 * @param client Discord client
 * @param interaction_id Interaction ID
 * @param interaction_token Interaction token
 * @param data_json JSON object for callback "data"
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_update_message_json(dc_client_t* client,
                                                      dc_snowflake_t interaction_id,
                                                      const char* interaction_token,
                                                      const char* data_json);

/**
 * @brief Open a modal in response to an interaction (callback type 9)
 * @param client Discord client
 * @param interaction_id Interaction ID
 * @param interaction_token Interaction token
 * @param modal_json JSON object for callback "data" modal payload
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_show_modal_json(dc_client_t* client,
                                                  dc_snowflake_t interaction_id,
                                                  const char* interaction_token,
                                                  const char* modal_json);

/**
 * @brief Respond to an interaction with a message
 * @param client Discord client
 * @param interaction_id Interaction ID
 * @param interaction_token Interaction token
 * @param content Message content
 * @param ephemeral Non-zero to send ephemeral response
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_respond_message(dc_client_t* client,
                                                  dc_snowflake_t interaction_id,
                                                  const char* interaction_token,
                                                  const char* content,
                                                  int ephemeral);

/**
 * @brief Edit the original interaction response using raw JSON
 * @param client Discord client
 * @param application_id Application ID
 * @param interaction_token Interaction token
 * @param json_body JSON body for PATCH /webhooks/{application.id}/{interaction.token}/messages/@original
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_edit_original_response_json(dc_client_t* client,
                                                              dc_snowflake_t application_id,
                                                              const char* interaction_token,
                                                              const char* json_body);

/**
 * @brief Edit the original interaction response
 * @param client Discord client
 * @param application_id Application ID
 * @param interaction_token Interaction token
 * @param content New message content
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_edit_original_response(dc_client_t* client,
                                                         dc_snowflake_t application_id,
                                                         const char* interaction_token,
                                                         const char* content);

/**
 * @brief Delete the original interaction response
 * @param client Discord client
 * @param application_id Application ID
 * @param interaction_token Interaction token
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_delete_original_response(dc_client_t* client,
                                                           dc_snowflake_t application_id,
                                                           const char* interaction_token);

/**
 * @brief Create a followup message for an interaction using raw JSON
 * @param client Discord client
 * @param application_id Application ID
 * @param interaction_token Interaction token
 * @param json_body JSON body for POST /webhooks/{application.id}/{interaction.token}
 * @param message_id Optional output for created followup message ID
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_create_followup_message_json(dc_client_t* client,
                                                               dc_snowflake_t application_id,
                                                               const char* interaction_token,
                                                               const char* json_body,
                                                               dc_snowflake_t* message_id);

/**
 * @brief Create a followup message for an interaction
 * @param client Discord client
 * @param application_id Application ID
 * @param interaction_token Interaction token
 * @param content Message content
 * @param ephemeral Non-zero to send ephemeral response
 * @param message_id Optional output for created followup message ID
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_create_followup_message(dc_client_t* client,
                                                          dc_snowflake_t application_id,
                                                          const char* interaction_token,
                                                          const char* content,
                                                          int ephemeral,
                                                          dc_snowflake_t* message_id);

/**
 * @brief Edit an interaction followup message using raw JSON
 * @param client Discord client
 * @param application_id Application ID
 * @param interaction_token Interaction token
 * @param message_id Followup message ID
 * @param json_body JSON body for PATCH /webhooks/{application.id}/{interaction.token}/messages/{message.id}
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_edit_followup_message_json(dc_client_t* client,
                                                             dc_snowflake_t application_id,
                                                             const char* interaction_token,
                                                             dc_snowflake_t message_id,
                                                             const char* json_body);

/**
 * @brief Edit an interaction followup message
 * @param client Discord client
 * @param application_id Application ID
 * @param interaction_token Interaction token
 * @param message_id Followup message ID
 * @param content New message content
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_edit_followup_message(dc_client_t* client,
                                                        dc_snowflake_t application_id,
                                                        const char* interaction_token,
                                                        dc_snowflake_t message_id,
                                                        const char* content);

/**
 * @brief Delete an interaction followup message
 * @param client Discord client
 * @param application_id Application ID
 * @param interaction_token Interaction token
 * @param message_id Followup message ID
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_interaction_delete_followup_message(dc_client_t* client,
                                                          dc_snowflake_t application_id,
                                                          const char* interaction_token,
                                                          dc_snowflake_t message_id);

/**
 * @brief Request guild members over Gateway (op 8)
 * @param client Discord client
 * @param guild_id Guild ID
 * @param query Username prefix query, or "" for all members (mutually exclusive with user_ids)
 * @param limit Max members for query mode
 * @param presences Non-zero to request presence objects
 * @param user_ids Optional user IDs (mutually exclusive with query)
 * @param user_id_count Number of user IDs
 * @param nonce Optional nonce (max 32 bytes)
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_request_guild_members(dc_client_t* client,
                                            dc_snowflake_t guild_id,
                                            const char* query,
                                            uint32_t limit,
                                            int presences,
                                            const dc_snowflake_t* user_ids,
                                            size_t user_id_count,
                                            const char* nonce);

/**
 * @brief Request soundboard sounds over Gateway (op 31)
 * @param client Discord client
 * @param guild_ids Guild ID array
 * @param guild_id_count Number of guild IDs
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_request_soundboard_sounds(dc_client_t* client,
                                                const dc_snowflake_t* guild_ids,
                                                size_t guild_id_count);

/**
 * @brief Update voice state over Gateway (op 4)
 * @param client Discord client
 * @param guild_id Guild ID
 * @param channel_id Voice channel ID, or 0 to disconnect
 * @param self_mute Non-zero to self-mute
 * @param self_deaf Non-zero to self-deafen
 * @return DC_OK on success, error code on failure
 */
dc_status_t dc_client_update_voice_state(dc_client_t* client,
                                         dc_snowflake_t guild_id,
                                         dc_snowflake_t channel_id,
                                         int self_mute,
                                         int self_deaf);

#ifdef __cplusplus
}
#endif

#endif /* DC_CLIENT_H */
