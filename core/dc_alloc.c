#include "dc_alloc.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdalign.h>
#if defined(DC_USE_GLIB_ALLOC)
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtraditional-conversion"
#endif
#include <glib.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif

typedef enum {
    DC_ALLOC_BACKEND_CUSTOM = 0,
    DC_ALLOC_BACKEND_LIBC,
    DC_ALLOC_BACKEND_GLIB
} dc_alloc_backend_t;

#if defined(DC_USE_GLIB_ALLOC)
#  define DC_GLIB_ALLOC_FN   g_try_malloc
#  define DC_GLIB_REALLOC_FN g_try_realloc
#  define DC_GLIB_FREE_FN    g_free
#  define DC_GLIB_ALLOC0_FN  g_try_malloc0
#endif

static dc_alloc_hooks_t g_hooks = {
#if defined(DC_USE_GLIB_ALLOC)
    .alloc = DC_GLIB_ALLOC_FN,
    .realloc = DC_GLIB_REALLOC_FN,
    .free = DC_GLIB_FREE_FN
#else
    .alloc = malloc,
    .realloc = realloc,
    .free = free
#endif
};


static inline int dc_is_standard_libc(void) {
    return (g_hooks.alloc == malloc && g_hooks.realloc == realloc && g_hooks.free == free);
}

static dc_alloc_backend_t g_backend =
#if defined(DC_USE_GLIB_ALLOC)
    DC_ALLOC_BACKEND_GLIB;
#else
    DC_ALLOC_BACKEND_LIBC;
#endif

static void dc_alloc_update_backend(void) {
    if (g_hooks.alloc == malloc && g_hooks.realloc == realloc && g_hooks.free == free) {
        g_backend = DC_ALLOC_BACKEND_LIBC;
        return;
    }
#if defined(DC_USE_GLIB_ALLOC)
    if (g_hooks.alloc == DC_GLIB_ALLOC_FN &&
        g_hooks.realloc == DC_GLIB_REALLOC_FN &&
        g_hooks.free == DC_GLIB_FREE_FN) {
        g_backend = DC_ALLOC_BACKEND_GLIB;
        return;
    }
#endif
    g_backend = DC_ALLOC_BACKEND_CUSTOM;
}


#if defined(__GNUC__) || defined(__clang__)
#  define DC_LIKELY(x)   __builtin_expect((long)!!(x), 1L)
#  define DC_UNLIKELY(x) __builtin_expect((long)!!(x), 0L)
#else
#  define DC_LIKELY(x)   (x)
#  define DC_UNLIKELY(x) (x)
#endif

static inline int dc_mul_overflow(size_t a, size_t b, size_t* res) {
#if defined(__GNUC__) && __GNUC__ >= 5
    return __builtin_mul_overflow(a, b, res);
#elif defined(__has_builtin)
#  if __has_builtin(__builtin_mul_overflow)
    return __builtin_mul_overflow(a, b, res);
#  endif
#endif
    if (b != 0 && a > SIZE_MAX / b) {
        return 1;
    }
    *res = a * b;
    return 0;
}

static inline void dc_bzero(void* ptr, size_t len) {
    if (!ptr || len == 0) return;
#if defined(__STDC_LIB_EXT1__)
    (void)memset_s(ptr, len, 0, len);
#else
    memset(ptr, 0, len);
#endif
}

dc_status_t dc_alloc_set_hooks(const dc_alloc_hooks_t* hooks) {
    if (DC_UNLIKELY(!hooks || !hooks->alloc || !hooks->realloc || !hooks->free)) {
        return DC_ERROR_INVALID_PARAM;
    }

    g_hooks = *hooks;
    dc_alloc_update_backend();
    return DC_OK;
}

dc_status_t dc_alloc_get_hooks(dc_alloc_hooks_t* hooks) {
    if (DC_UNLIKELY(!hooks)) {
        return DC_ERROR_NULL_POINTER;
    }

    *hooks = g_hooks;
    return DC_OK;
}

void dc_alloc_reset_hooks(void) {
#if defined(DC_USE_GLIB_ALLOC)
    g_hooks.alloc = DC_GLIB_ALLOC_FN;
    g_hooks.realloc = DC_GLIB_REALLOC_FN;
    g_hooks.free = DC_GLIB_FREE_FN;
#else
    g_hooks.alloc = malloc;
    g_hooks.realloc = realloc;
    g_hooks.free = free;
#endif
    dc_alloc_update_backend();
}

void* dc_alloc(size_t size) {
    if (DC_UNLIKELY(size == 0)) {
        return NULL;
    }

    return g_hooks.alloc(size);
}

void* dc_calloc(size_t count, size_t size) {
    if (DC_UNLIKELY(count == 0 || size == 0)) {
        return NULL;
    }
    
    size_t total_size;
    if (DC_UNLIKELY(dc_mul_overflow(count, size, &total_size))) {
        return NULL;
    }
    if (g_backend == DC_ALLOC_BACKEND_LIBC) {
        return calloc(count, size);
    }
#if defined(DC_USE_GLIB_ALLOC)
    if (g_backend == DC_ALLOC_BACKEND_GLIB) {
        return DC_GLIB_ALLOC0_FN(total_size);
    }
#endif
    if (dc_is_standard_libc()) {
        return calloc(count, size);
    }
    void* ptr = g_hooks.alloc(total_size);
    if (DC_LIKELY(ptr)) {
        dc_bzero(ptr, total_size);
    }
    
    return ptr;
}

void* dc_realloc(void* ptr, size_t size) {
    if (DC_UNLIKELY(size == 0)) {
        if (DC_LIKELY(ptr)) {
            g_hooks.free(ptr);
        }
        return NULL;
    }

    if (DC_UNLIKELY(!ptr)) {
        return g_hooks.alloc(size);
    }

    return g_hooks.realloc(ptr, size);
}

void dc_free(void* ptr) {
    if (DC_LIKELY(ptr)) {
        g_hooks.free(ptr);
    }
}

char* dc_strdup(const char* str) {
    if (DC_UNLIKELY(!str)) {
        return NULL;
    }
    size_t len = strlen(str);
    char* dup = dc_alloc(len + 1);
    if (DC_LIKELY(dup)) {
        memcpy(dup, str, len + 1);
    }

    return dup;
}

char* dc_strndup(const char* str, size_t max_len) {
    if (DC_UNLIKELY(!str)) {
        return NULL;
    }
    size_t len = 0;
    while (DC_LIKELY(len < max_len && str[len] != '\0')) {
        len++;
    }

    char* dup = dc_alloc(len + 1);
    if (DC_LIKELY(dup)) {
        memcpy(dup, str, len);
        dup[len] = '\0';
    }

    return dup;
}

void* dc_alloc_aligned(size_t size, size_t alignment) {
    if (DC_UNLIKELY(size == 0 || alignment == 0)) {
        return NULL;
    }
    if (DC_UNLIKELY((alignment & (alignment - 1)) != 0)) {
        return NULL;
    }
#if defined(_ISOC11_SOURCE) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
    if (g_hooks.alloc == malloc && g_hooks.realloc == realloc && g_hooks.free == free) {
        if (size % alignment != 0) {
            return NULL;
        }
        return aligned_alloc(alignment, size);
    }
#endif
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    if (g_hooks.alloc == malloc && g_hooks.realloc == realloc && g_hooks.free == free) {
        void* ptr = NULL;
        if (alignment < sizeof(void*) || (alignment % sizeof(void*)) != 0) {
            return NULL;
        }
        if (posix_memalign(&ptr, alignment, size) != 0) {
            return NULL;
        }
        return ptr;
    }
    return NULL;
#else
    return NULL;
#endif
}
