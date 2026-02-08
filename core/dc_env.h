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
 * @brief When loading a dotenv file, override existing process env vars.
 */
#define DC_ENV_FLAG_OVERRIDE_EXISTING 0x2u

/**
 * @brief When loading a dotenv file, allow setting empty values (`KEY=`).
 */
#define DC_ENV_FLAG_ALLOW_EMPTY 0x4u

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
 * @brief Resolve a variable from process env first, then auto-discover a dotenv file.
 *
 * Discovery order when process env is missing:
 * 1) `DC_DOTENV_PATH` process env var, if set
 * 2) Walk up from current working directory and look for @p dotenv_filename
 *
 * @param dotenv_filename Dotenv filename to search for (NULL/empty => ".env")
 * @param max_depth Maximum number of parent traversals (0 => only CWD)
 * @param flags Bitmask of DC_ENV_FLAG_* values (applies to file lookup)
 */
dc_status_t dc_env_get_with_dotenv_search(const char* key,
                                         const char* dotenv_filename,
                                         size_t max_depth,
                                         unsigned flags,
                                         dc_string_t* out_value);

/**
 * @brief Resolve DISCORD_TOKEN using process env and auto-discovered dotenv file.
 */
dc_status_t dc_env_get_discord_token_auto(unsigned flags, dc_string_t* out_token);

/**
 * @brief Load dotenv-style file into the process environment.
 *
 * If @p dotenv_path is NULL/empty, it will try `DC_DOTENV_PATH` and then search
 * for ".env" by walking up from the current working directory.
 *
 * @param dotenv_path Absolute/relative path to dotenv file (may start with "~/" on POSIX)
 * @param flags Bitmask of DC_ENV_FLAG_* values (REQUIRE_PRIVATE_FILE, OVERRIDE_EXISTING, ALLOW_EMPTY)
 * @param out_loaded Optional count of variables set in the process env
 */
dc_status_t dc_env_load_dotenv(const char* dotenv_path,
                               unsigned flags,
                               size_t* out_loaded);

/**
 * @brief Overwrite string bytes and reset it.
 */
void dc_env_secure_clear_string(dc_string_t* value);

#ifdef __cplusplus
}
#endif

#endif /* DC_ENV_H */
