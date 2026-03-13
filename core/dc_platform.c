/**
 * @file dc_platform.c
 * @brief Internal cross-platform runtime helpers.
 */

#include "core/dc_platform.h"

#include <errno.h>

#if !defined(_WIN32)
#include <time.h>
#endif

int dc_platform_now_monotonic_ms(uint64_t* out_ms) {
    if (!out_ms) return 0;
#if defined(_WIN32)
    *out_ms = (uint64_t)GetTickCount64();
    return 1;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    *out_ms = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
    return 1;
#endif
}

int dc_platform_now_epoch_ms(uint64_t* out_ms) {
    if (!out_ms) return 0;
#if defined(_WIN32)
    FILETIME ft;
    ULARGE_INTEGER ticks;
    const uint64_t epoch_bias_100ns = 116444736000000000ULL;
    GetSystemTimeAsFileTime(&ft);
    ticks.LowPart = ft.dwLowDateTime;
    ticks.HighPart = ft.dwHighDateTime;
    if (ticks.QuadPart < epoch_bias_100ns) return 0;
    *out_ms = (ticks.QuadPart - epoch_bias_100ns) / 10000ULL;
    return 1;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return 0;
    *out_ms = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
    return 1;
#endif
}

void dc_platform_sleep_ms(uint64_t ms) {
    if (ms == 0) return;
#if defined(_WIN32)
    while (ms > 0) {
        DWORD chunk = (ms > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : (DWORD)ms;
        Sleep(chunk);
        ms -= (uint64_t)chunk;
    }
#else
    struct timespec req;
    req.tv_sec = (time_t)(ms / 1000ULL);
    req.tv_nsec = (long)((ms % 1000ULL) * 1000000ULL);
    while (nanosleep(&req, &req) != 0 && errno == EINTR) {
    }
#endif
}

int dc_platform_localtime_safe(const time_t* t, struct tm* out) {
    if (!t || !out) return 0;
#if defined(_WIN32) || defined(__STDC_LIB_EXT1__)
    return localtime_s(out, t) == 0;
#elif defined(_POSIX_VERSION) || defined(__unix__) || defined(__APPLE__)
    return localtime_r(t, out) != NULL;
#else
    struct tm* tmp = localtime(t);
    if (!tmp) return 0;
    *out = *tmp;
    return 1;
#endif
}

int dc_platform_gmtime_safe(const time_t* t, struct tm* out) {
    if (!t || !out) return 0;
#if defined(_WIN32) || defined(__STDC_LIB_EXT1__)
    return gmtime_s(out, t) == 0;
#elif defined(_POSIX_VERSION) || defined(__unix__) || defined(__APPLE__)
    return gmtime_r(t, out) != NULL;
#else
    struct tm* tmp = gmtime(t);
    if (!tmp) return 0;
    *out = *tmp;
    return 1;
#endif
}

int dc_platform_mutex_init(dc_platform_mutex_t* mutex) {
    if (!mutex) return 0;
#if defined(_WIN32)
    InitializeSRWLock(mutex);
    return 1;
#else
    return pthread_mutex_init(mutex, NULL) == 0;
#endif
}

void dc_platform_mutex_destroy(dc_platform_mutex_t* mutex) {
    if (!mutex) return;
#if !defined(_WIN32)
    (void)pthread_mutex_destroy(mutex);
#else
    (void)mutex;
#endif
}

int dc_platform_mutex_lock(dc_platform_mutex_t* mutex) {
    if (!mutex) return 0;
#if defined(_WIN32)
    AcquireSRWLockExclusive(mutex);
    return 1;
#else
    return pthread_mutex_lock(mutex) == 0;
#endif
}

int dc_platform_mutex_unlock(dc_platform_mutex_t* mutex) {
    if (!mutex) return 0;
#if defined(_WIN32)
    ReleaseSRWLockExclusive(mutex);
    return 1;
#else
    return pthread_mutex_unlock(mutex) == 0;
#endif
}
