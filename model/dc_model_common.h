#ifndef DC_MODEL_COMMON_H
#define DC_MODEL_COMMON_H

/**
 * @file dc_model_common.h
 * @brief Common helpers for model optional/nullable fields
 */

#include <string.h>
#include <stdint.h>
#include "core/dc_status.h"
#include "core/dc_string.h"
#include "core/dc_optional.h"
#include "core/dc_snowflake.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef DC_OPTIONAL(dc_snowflake_t) dc_optional_snowflake_t;
typedef DC_NULLABLE(dc_snowflake_t) dc_nullable_snowflake_t;
typedef DC_OPTIONAL(int32_t) dc_optional_i32_t;
typedef DC_OPTIONAL(int) dc_optional_bool_t;

typedef struct {
    int is_set;
    uint64_t value;
} dc_optional_u64_field_t;

typedef struct {
    int is_set;
    dc_string_t value;
} dc_optional_string_t;

typedef struct {
    int is_null;
    dc_string_t value;
} dc_nullable_string_t;

static inline dc_status_t dc_optional_string_init(dc_optional_string_t* opt) {
    if (!opt) return DC_ERROR_NULL_POINTER;
    opt->is_set = 0;
    return dc_string_init(&opt->value);
}

static inline void dc_optional_string_free(dc_optional_string_t* opt) {
    if (!opt) return;
    dc_string_free(&opt->value);
    opt->is_set = 0;
}

static inline dc_status_t dc_nullable_string_init(dc_nullable_string_t* opt) {
    if (!opt) return DC_ERROR_NULL_POINTER;
    opt->is_null = 1;
    return dc_string_init(&opt->value);
}

static inline void dc_nullable_string_free(dc_nullable_string_t* opt) {
    if (!opt) return;
    dc_string_free(&opt->value);
    opt->is_null = 1;
}

static inline void dc_optional_snowflake_clear(dc_optional_snowflake_t* opt) {
    if (!opt) return;
    opt->is_set = 0;
    opt->value = 0;
}

static inline void dc_nullable_snowflake_set_null(dc_nullable_snowflake_t* opt) {
    if (!opt) return;
    opt->is_null = 1;
    opt->value = 0;
}

static inline void dc_optional_i32_clear(dc_optional_i32_t* opt) {
    if (!opt) return;
    opt->is_set = 0;
    opt->value = 0;
}

static inline void dc_optional_bool_clear(dc_optional_bool_t* opt) {
    if (!opt) return;
    opt->is_set = 0;
    opt->value = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* DC_MODEL_COMMON_H */
