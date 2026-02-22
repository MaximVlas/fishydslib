#ifndef DC_ALLOC_H
#define DC_ALLOC_H

/**
 * @file dc_alloc.h
 * @brief Memory allocation wrappers with pluggable backends and safety guards.
 *
 * This module wraps the standard C allocation functions (malloc, realloc, free)
 * behind a thin hook layer that lets the caller swap in a custom or third-party
 * allocator at runtime — for example GLib's g_try_malloc — without changing any
 * call sites in the rest of the library.
 *
 * Three backends are supported, selected at compile time or switched at runtime:
 *
 *   DC_ALLOC_BACKEND_LIBC   - Standard C library: malloc / realloc / free.
 *                             (ISO/IEC 9899:2011 §7.22.3)
 *                             Default when DC_USE_GLIB_ALLOC is not defined.
 *
 *   DC_ALLOC_BACKEND_GLIB   - GLib: g_try_malloc / g_try_realloc / g_free.
 *                             Active when compiled with -DDC_USE_GLIB_ALLOC.
 *                             g_try_* variants return NULL on failure rather
 *                             than aborting, matching the libc contract here.
 *
 *   DC_ALLOC_BACKEND_CUSTOM - Any user-supplied alloc/realloc/free triple
 *                             installed via dc_alloc_set_hooks().
 *
 * Safety properties provided by this layer (beyond raw malloc/realloc/free):
 *
 *   - Zero-size guards: dc_alloc(), dc_calloc(), dc_realloc() all return NULL
 *     immediately for size == 0, avoiding implementation-defined behaviour
 *     described in ISO C11 §7.22.3.4 and §7.22.3.5.
 *
 *   - Overflow detection: dc_calloc() checks count * size for multiplication
 *     overflow before allocating, using __builtin_mul_overflow on GCC >= 5 /
 *     Clang, and a portable SIZE_MAX / b > a fallback otherwise.
 *     (GCC Integer Overflow Builtins: https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html)
 *
 *   - NULL-safe free: dc_free() silently ignores NULL pointers, consistent
 *     with ISO C11 §7.22.3.3 ("If ptr is a null pointer, no action occurs").
 *
 *   - Semantic realloc: dc_realloc(ptr, 0) frees ptr and returns NULL, and
 *     dc_realloc(NULL, size) behaves like dc_alloc(size), making both edge
 *     cases explicit and predictable regardless of the active backend.
 *
 * Branch prediction hints (DC_LIKELY / DC_UNLIKELY) are applied throughout
 * the hot paths using GCC/Clang's __builtin_expect. These are no-ops on
 * compilers that do not support them, so they do not affect correctness.
 * (GCC built-in: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html)
 *
 * Thread safety:
 *   dc_alloc_set_hooks() and dc_alloc_reset_hooks() modify a global state and
 *   are NOT thread-safe. They must be called before spawning threads, or
 *   protected externally. All allocation functions themselves are as thread-safe
 *   as the underlying backend (libc malloc is MT-safe on POSIX systems;
 *   GLib's g_try_malloc is also MT-safe).
 *
 * Implementation file: dc_alloc.c
 */

#include <stddef.h>
#include "dc_status.h"

#ifdef __cplusplus
extern "C" {
#endif


/* =========================================================================
 * Function pointer types
 * ========================================================================= */

/**
 * @brief Signature of an allocation function (analogous to malloc).
 *
 * Must return a pointer to at least `size` bytes of uninitialized memory on
 * success, or NULL on failure. Behaviour for size == 0 is implementation-
 * defined by ISO C11 §7.22.3; dc_alloc() guards against size == 0 before
 * calling through this pointer, so the hook will never be called with size 0.
 *
 * @param size  Number of bytes to allocate.
 * @return Pointer to allocated memory, or NULL on failure.
 */
typedef void* (*dc_alloc_fn_t)(size_t size);

/**
 * @brief Signature of a reallocation function (analogous to realloc).
 *
 * Must resize the allocation at `ptr` to `size` bytes and return a (possibly
 * different) pointer to the resized region, or NULL on failure without freeing
 * the original block. If ptr is NULL the function must behave like alloc(size).
 * Per ISO C11 §7.22.3.5, if size is 0 the behaviour is implementation-defined;
 * dc_realloc() handles that edge case itself and will never call this with
 * size == 0.
 *
 * @param ptr   Pointer to an existing allocation, or NULL.
 * @param size  New size in bytes.
 * @return Pointer to the resized allocation, or NULL on failure.
 */
typedef void* (*dc_realloc_fn_t)(void* ptr, size_t size);

/**
 * @brief Signature of a deallocation function (analogous to free).
 *
 * Must release the memory pointed to by `ptr`, which must have been returned
 * by a previous call to the paired alloc or realloc function. Behaviour for
 * ptr == NULL is implementation-defined; dc_free() guards against NULL before
 * calling through this pointer, so the hook will never be called with NULL.
 * (ISO C11 §7.22.3.3 guarantees that free(NULL) is a no-op for libc itself;
 * custom hooks may not share this guarantee, hence the guard.)
 *
 * @param ptr  Pointer to memory to release.
 */
typedef void (*dc_free_fn_t)(void* ptr);


/* =========================================================================
 * Hook structure
 * ========================================================================= */

/**
 * @brief A set of three allocation function pointers forming one allocator.
 *
 * All three fields must point to compatible functions — that is, memory
 * allocated through `alloc` or `realloc` must be releasable through `free`.
 * Mixing pointers from different allocators (e.g. alloc from GLib, free from
 * libc) produces undefined behaviour.
 *
 * Passed to dc_alloc_set_hooks() to install a custom allocator globally.
 * Retrieved via dc_alloc_get_hooks() to inspect the currently active one.
 */
typedef struct {
    dc_alloc_fn_t   alloc;    /**< Allocation function; must not be NULL.   */
    dc_realloc_fn_t realloc;  /**< Reallocation function; must not be NULL. */
    dc_free_fn_t    free;     /**< Deallocation function; must not be NULL. */
} dc_alloc_hooks_t;


/* =========================================================================
 * Hook management
 * ========================================================================= */

/**
 * @brief Replace the active allocator with a custom alloc/realloc/free triple.
 *
 * All subsequent calls to dc_alloc(), dc_calloc(), dc_realloc(), dc_free(),
 * dc_strdup(), dc_strndup(), and dc_alloc_aligned() will use the new hooks.
 *
 * The internal backend tag (LIBC / GLIB / CUSTOM) is updated automatically.
 * dc_calloc() uses this tag to route zero-initialisation: if the backend is
 * LIBC it delegates directly to calloc(3) (which may be faster than malloc +
 * memset on some platforms); if GLIB it uses g_try_malloc0; otherwise it
 * falls back to alloc + memset.
 *
 * @warning NOT thread-safe. Must be called before any threads that use the
 *          allocator are spawned, or protected by an external lock.
 *
 * @param hooks  Pointer to a fully-populated dc_alloc_hooks_t. All three
 *               function pointers must be non-NULL.
 * @return DC_OK on success.
 * @return DC_ERROR_INVALID_PARAM if hooks is NULL or any function pointer
 *         within it is NULL.
 */
dc_status_t dc_alloc_set_hooks(const dc_alloc_hooks_t* hooks);

/**
 * @brief Copy the currently active allocator hooks into a caller-supplied
 *        dc_alloc_hooks_t.
 *
 * Useful when the caller wants to wrap the existing allocator (e.g. to add
 * logging or instrumentation) without needing to know which backend is active.
 *
 * @param hooks  Output structure. Must not be NULL.
 * @return DC_OK on success.
 * @return DC_ERROR_NULL_POINTER if hooks is NULL.
 */
dc_status_t dc_alloc_get_hooks(dc_alloc_hooks_t* hooks);

/**
 * @brief Restore the allocator to the compile-time default.
 *
 * If compiled with -DDC_USE_GLIB_ALLOC, restores to g_try_malloc /
 * g_try_realloc / g_free. Otherwise restores to malloc / realloc / free.
 *
 * @warning NOT thread-safe. Same constraints as dc_alloc_set_hooks().
 */
void dc_alloc_reset_hooks(void);


/* =========================================================================
 * Allocation functions
 * ========================================================================= */

/**
 * @brief Allocate `size` bytes of uninitialized memory.
 *
 * A thin wrapper around the active alloc hook that adds a zero-size guard.
 * Memory content is indeterminate (not zeroed); use dc_calloc() if you need
 * zero-initialized memory.
 *
 * Corresponds to malloc(3) from ISO C11 §7.22.3.4, with the difference that
 * size == 0 returns NULL deterministically rather than exhibiting
 * implementation-defined behaviour.
 *
 * @param size  Number of bytes to allocate. Returns NULL if 0.
 * @return Pointer to allocated memory on success, NULL on failure or size == 0.
 */
void* dc_alloc(size_t size);

/**
 * @brief Allocate an array of `count` elements of `size` bytes each,
 *        zero-initialized.
 *
 * Checks count * size for multiplication overflow before allocating.
 * On GCC >= 5 and Clang, this uses __builtin_mul_overflow for a single
 * hardware instruction on supported architectures. On other compilers it
 * falls back to the portable check: b != 0 && a > SIZE_MAX / b.
 *
 * Zero-initialisation routing:
 *   - LIBC backend  → calloc(count, size)       (may be faster than malloc+memset)
 *   - GLIB backend  → g_try_malloc0(total_size)
 *   - CUSTOM backend → active alloc hook + memset(ptr, 0, total_size)
 *
 * Corresponds to calloc(3) from ISO C11 §7.22.3.2.
 *
 * @param count  Number of elements. Returns NULL if 0.
 * @param size   Size of each element in bytes. Returns NULL if 0.
 * @return Pointer to zero-initialized memory on success, NULL on failure,
 *         size/count == 0, or integer overflow in count * size.
 */
void* dc_calloc(size_t count, size_t size);

/**
 * @brief Resize an existing allocation to `size` bytes.
 *
 * Adds two edge-case guards on top of the raw realloc hook:
 *
 *   dc_realloc(ptr, 0)     → frees ptr, returns NULL.
 *   dc_realloc(NULL, size) → equivalent to dc_alloc(size).
 *
 * These make the behaviour explicit and portable, since ISO C11 §7.22.3.5
 * specifies that realloc(ptr, 0) is implementation-defined. The existing
 * contents of the allocation are preserved up to min(old_size, size) bytes.
 *
 * @param ptr   Pointer to an existing allocation, or NULL.
 * @param size  New size in bytes. If 0, ptr is freed and NULL is returned.
 * @return Pointer to the resized allocation, NULL on failure or size == 0.
 */
void* dc_realloc(void* ptr, size_t size);

/**
 * @brief Free memory previously allocated by any dc_alloc* function.
 *
 * Silently ignores NULL pointers. This matches the ISO C11 §7.22.3.3
 * guarantee for free(NULL) but is also safe when the custom hook's free
 * does not honour that guarantee, since the NULL check happens before
 * the hook is called.
 *
 * Every pointer returned by dc_alloc(), dc_calloc(), dc_realloc(),
 * dc_strdup(), dc_strndup(), and dc_alloc_aligned() must eventually be
 * passed to dc_free(). Do NOT mix dc_free() with free() from a different
 * allocator.
 *
 * @param ptr  Pointer to free, or NULL (no-op).
 */
void dc_free(void* ptr);


/* =========================================================================
 * String duplication
 * ========================================================================= */

/**
 * @brief Duplicate a null-terminated C string using the active allocator.
 *
 * Allocates strlen(str) + 1 bytes and copies the string including the null
 * terminator. Equivalent to POSIX strdup(3) but routes through dc_alloc()
 * so that the returned pointer must be freed with dc_free(), not free().
 *
 * @param str  String to duplicate. Returns NULL if NULL.
 * @return Newly allocated copy of str, or NULL on allocation failure or
 *         if str is NULL. The caller owns the returned pointer.
 */
char* dc_strdup(const char* str);

/**
 * @brief Duplicate up to `max_len` characters of a string using the active
 *        allocator.
 *
 * Copies at most max_len characters from str, then appends a null terminator.
 * If str is shorter than max_len, the full string is copied. The result is
 * always null-terminated.
 *
 * Equivalent to POSIX strndup(3) but routes through dc_alloc(). Note that
 * unlike strncpy(3), the result is always null-terminated regardless of
 * whether the source string is shorter or longer than max_len.
 *
 * @param str      String to duplicate. Returns NULL if NULL.
 * @param max_len  Maximum number of characters to copy (not including the
 *                 null terminator). The allocation will be max_len + 1 bytes
 *                 in the worst case.
 * @return Newly allocated, null-terminated string, or NULL on failure or if
 *         str is NULL. The caller owns the returned pointer.
 */
char* dc_strndup(const char* str, size_t max_len);


/* =========================================================================
 * Aligned allocation
 * ========================================================================= */

/**
 * @brief Allocate `size` bytes aligned to an `alignment`-byte boundary.
 *
 * Only works when the active backend is the standard libc (malloc/realloc/free).
 * Returns NULL immediately if a custom or GLib backend is active — there is no
 * portable way to guarantee alignment with an arbitrary allocator.
 *
 * Two underlying mechanisms are tried in order, depending on the build:
 *
 *   1. C11 aligned_alloc (ISO/IEC 9899:2011 §7.22.3.1):
 *      Available when _ISOC11_SOURCE or __STDC_VERSION__ >= 201112L.
 *      Requires size to be a multiple of alignment (C11 DR 460 clarified that
 *      non-multiples return NULL rather than invoking undefined behaviour).
 *      Returns NULL if size % alignment != 0.
 *
 *   2. POSIX posix_memalign (IEEE Std 1003.1-2008 §posix_memalign):
 *      Available when _POSIX_C_SOURCE >= 200112L.
 *      Requires alignment to be a power of two AND a multiple of sizeof(void*).
 *      Returns NULL if alignment < sizeof(void*) or alignment % sizeof(void*) != 0.
 *
 * Alignment must be a non-zero power of two in all cases. The power-of-two
 * check is performed before any backend call: (alignment & (alignment - 1)) != 0
 * detects non-powers-of-two via the standard bitmask trick.
 *
 * Typical use cases: SSE/AVX SIMD buffers (16/32-byte alignment), cache-line
 * aligned structures (typically 64 bytes), or DMA buffers (page-aligned).
 *
 * The returned pointer must be freed with dc_free(), which routes to free(3)
 * when the libc backend is active. POSIX guarantees that memory from
 * posix_memalign() can be released with free(3).
 *
 * @param size       Number of bytes to allocate. Returns NULL if 0.
 * @param alignment  Required alignment in bytes. Must be a non-zero power of
 *                   two. For aligned_alloc, size must also be a multiple of
 *                   alignment. For posix_memalign, alignment must additionally
 *                   be a multiple of sizeof(void*).
 * @return Pointer to aligned memory on success, NULL on failure or if any
 *         precondition is violated, or if a non-libc backend is active.
 */
void* dc_alloc_aligned(size_t size, size_t alignment);


#ifdef __cplusplus
}
#endif

#endif /* DC_ALLOC_H */