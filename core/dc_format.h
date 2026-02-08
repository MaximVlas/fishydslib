#ifndef DC_FORMAT_H
#define DC_FORMAT_H

/**
 * @file dc_format.h
 * @brief Message formatting helpers (mentions, timestamps, escaping)
 *
 * @note Prefer allowed_mentions for mention control; escaping here is best-effort.
 * @note Use allowed_mentions to suppress mentions (e.g., parse = []).
 * @note Output strings must be initialized; contents are replaced on success.
 */

#include <stdint.h>
#include "dc_status.h"
#include "dc_string.h"
#include "dc_snowflake.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if a timestamp style character is valid
 * @param style Style character ('t','T','d','D','f','F','R') or '\0' for default
 * @return 1 if valid, 0 otherwise
 */
int dc_format_timestamp_style_is_valid(char style);

/**
 * @brief Build a user mention (<@id>)
 */
dc_status_t dc_format_mention_user(dc_snowflake_t user_id, dc_string_t* out);

/**
 * @brief Build a nickname mention (<@!id>)
 */
dc_status_t dc_format_mention_user_nick(dc_snowflake_t user_id, dc_string_t* out);

/**
 * @brief Build a channel mention (<#id>)
 */
dc_status_t dc_format_mention_channel(dc_snowflake_t channel_id, dc_string_t* out);

/**
 * @brief Build a role mention (<@&id>)
 */
dc_status_t dc_format_mention_role(dc_snowflake_t role_id, dc_string_t* out);

/**
 * @brief Build a slash command mention (</name:id>)
 * @param name Command name (may include spaces for subcommands)
 */
dc_status_t dc_format_slash_command_mention(const char* name, dc_snowflake_t command_id,
                                            dc_string_t* out);

/**
 * @brief Build an emoji mention (<:name:id> or <a:name:id>)
 * @param name Emoji name
 * @param emoji_id Emoji ID
 * @param animated Non-zero for animated emoji
 */
dc_status_t dc_format_mention_emoji(const char* name, dc_snowflake_t emoji_id, int animated,
                                    dc_string_t* out);

/**
 * @brief Build a timestamp mention (<t:unix[:style]>)
 * @param unix_seconds Unix timestamp in seconds (UTC)
 * @param style Style character or '\0' for default
 */
dc_status_t dc_format_timestamp(int64_t unix_seconds, char style, dc_string_t* out);

/**
 * @brief Build a timestamp mention from milliseconds
 * @param unix_ms Unix timestamp in milliseconds (UTC)
 * @param style Style character or '\0' for default
 */
dc_status_t dc_format_timestamp_ms(int64_t unix_ms, char style, dc_string_t* out);

/**
 * @brief Escape markdown control and mention prefix characters
 * @param input Input string
 * @param out Output string (receives escaped content)
 *
 * @note Escaping does not replace allowed_mentions; use both for safety.
 */
dc_status_t dc_format_escape_content(const char* input, dc_string_t* out);

#ifdef __cplusplus
}
#endif

#endif /* DC_FORMAT_H */
