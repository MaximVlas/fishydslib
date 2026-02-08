#ifndef DC_ENV_H
#define DC_ENV_H

/**
 * @file dc_env.h
 * @brief Safe environment and dotenv helpers.
 */

#include <stddef.h>
#include "dc_status.h"
#include "dc_string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Require dotenv files to be private to the current user.
 *
 * On POSIX this rejects files with any group/other permission bits set.
 */
#define DC_ENV_FLAG_REQUIRE_PRIVATE_FILE 0x1u

/**
 * @brief Read a variable from process environment.
 *
 * @return DC_OK when found and non-empty, DC_ERROR_NOT_FOUND otherwise.
 */
dc_status_t dc_env_get_process(const char* key, dc_string_t* out_value);

/**
 * @brief Read a variable from a dotenv-style file.
 *
 * Supported forms:
 * - `KEY=value`
 * - `export KEY=value`
 * - quoted values (`"value"` or `'value'`)
 * - blank lines and `#` comments
 *
 * @param flags Bitmask of DC_ENV_FLAG_* values
 * @return DC_OK when found and non-empty, DC_ERROR_NOT_FOUND otherwise.
 */
dc_status_t dc_env_get_from_file(const char* path,
                                 const char* key,
                                 unsigned flags,
                                 dc_string_t* out_value);

/**
 * @brief Resolve a variable from process env first, then fallback files.
 *
 * @param paths Array of dotenv file paths
 * @param path_count Number of entries in paths
 * @param flags Bitmask of DC_ENV_FLAG_* values for file lookups
 * @return DC_OK when found and non-empty, DC_ERROR_NOT_FOUND otherwise.
 */
dc_status_t dc_env_get_with_fallback(const char* key,
                                     const char* const* paths,
                                     size_t path_count,
                                     unsigned flags,
                                     dc_string_t* out_value);

/**
 * @brief Resolve DISCORD_TOKEN from process env and optional dotenv paths.
 *
 * @return DC_OK when found and non-empty, DC_ERROR_NOT_FOUND otherwise.
 */
dc_status_t dc_env_get_discord_token(const char* const* paths,
                                     size_t path_count,
                                     unsigned flags,
                                     dc_string_t* out_token);

/**
 * @brief Overwrite string bytes and reset it.
 */
void dc_env_secure_clear_string(dc_string_t* value);

#ifdef __cplusplus
}
#endif

#endif /* DC_ENV_H */
