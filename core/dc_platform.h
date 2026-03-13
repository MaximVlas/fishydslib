#ifndef DC_PLATFORM_H
#define DC_PLATFORM_H

/**
 * @file dc_platform.h
 * @brief Internal cross-platform runtime helpers.
 */

#include <stdint.h>
#include <time.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
typedef SRWLOCK dc_platform_mutex_t;
#else
#include <pthread.h>
typedef pthread_mutex_t dc_platform_mutex_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

int dc_platform_now_monotonic_ms(uint64_t* out_ms);
int dc_platform_now_epoch_ms(uint64_t* out_ms);
void dc_platform_sleep_ms(uint64_t ms);

int dc_platform_localtime_safe(const time_t* t, struct tm* out);
int dc_platform_gmtime_safe(const time_t* t, struct tm* out);

int dc_platform_mutex_init(dc_platform_mutex_t* mutex);
void dc_platform_mutex_destroy(dc_platform_mutex_t* mutex);
int dc_platform_mutex_lock(dc_platform_mutex_t* mutex);
int dc_platform_mutex_unlock(dc_platform_mutex_t* mutex);

#ifdef __cplusplus
}
#endif

#endif /* DC_PLATFORM_H */
