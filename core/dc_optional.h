#ifndef DC_OPTIONAL_H
#define DC_OPTIONAL_H

/**
 * @file dc_optional.h
 * @brief Helpers for optional and nullable fields in models and JSON
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Optional value wrapper (field may be absent)
 */
#define DC_OPTIONAL(T) struct { int is_set; T value; }

/**
 * @brief Nullable value wrapper (field present but may be null)
 */
#define DC_NULLABLE(T) struct { int is_null; T value; }

/**
 * @brief Optional helpers
 */
#define DC_OPTIONAL_CLEAR(opt) do { (opt).is_set = 0; } while (0)
#define DC_OPTIONAL_SET(opt, val) do { (opt).is_set = 1; (opt).value = (val); } while (0)

/**
 * @brief Nullable helpers
 */
#define DC_NULLABLE_SET(opt, val) do { (opt).is_null = 0; (opt).value = (val); } while (0)
#define DC_NULLABLE_SET_NULL(opt) do { (opt).is_null = 1; } while (0)

#ifdef __cplusplus
}
#endif

#endif /* DC_OPTIONAL_H */
