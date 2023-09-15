#ifndef AUX_THREADING_H
#define AUX_THREADING_H

#include "../platform.h"

#include <stdbool.h>
#include <stdint.h>

#if PLATFORM == PLAT_XBOX
    #define AUX_THREADING_STDC
#endif
#ifndef AUX_THREADING_STDC
    #include <pthread.h>
#else
    #include <threads.h>
#endif

struct thread_t;
struct thread_data {
    struct thread_t* self;
    void* args;
    bool shouldclose;
};
typedef void* (*threadfunc_t)(struct thread_data*);
typedef struct thread_t {
    #ifndef AUX_THREADING_STDC
    pthread_t thread;
    #else
    thrd_t thread;
    #endif
    char* name;
    threadfunc_t func;
    struct thread_data data;
} thread_t;
#ifndef AUX_THREADING_STDC
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;
#else
typedef mtx_t mutex_t;
typedef cnd_t cond_t;
#endif

bool createThread(thread_t*, const char* name, threadfunc_t func, void* args);
void destroyThread(thread_t*, void** ret);

#ifndef AUX_THREADING_STDC
    #define yield() sched_yield()
#else
    #define yield() thrd_yield()
#endif

static inline bool createMutex(mutex_t* m) {
    #ifndef AUX_THREADING_STDC
    return !pthread_mutex_init(m, NULL);
    #else
    return (mtx_init(m, mtx_plain) == thrd_success);
    #endif
}
static inline void lockMutex(mutex_t* m) {
    #ifndef AUX_THREADING_STDC
    while (pthread_mutex_lock(m)) {}
    #else
    #if PLATFORM != PLAT_XBOX
    while (mtx_lock(m) != thrd_success) {}
    #else
    // Ignore return value on Xbox. The NXDK doesn't check the return code of NtWaitForSingleObject correctly (mtx_lock
    // checks for STATUS_WAIT_0 instead of using NT_SUCCESS()).
    mtx_lock(m);
    #endif
    #endif
}
static inline void unlockMutex(mutex_t* m) {
    #ifndef AUX_THREADING_STDC
    while (pthread_mutex_unlock(m)) {}
    #else
    while (mtx_unlock(m) != thrd_success) {}
    #endif
}
static inline void destroyMutex(mutex_t* m) {
    #ifndef AUX_THREADING_STDC
    pthread_mutex_destroy(m);
    #else
    mtx_destroy(m);
    #endif
}

static inline bool createCond(cond_t* c) {
    #ifndef AUX_THREADING_STDC
    return !pthread_cond_init(c, NULL);
    #else
    return (cnd_init(c) == thrd_success);
    #endif
}
static inline void destroyCond(cond_t* c) {
    #ifndef AUX_THREADING_STDC
    pthread_cond_destroy(c);
    #else
    cnd_destroy(c);
    #endif
}
static inline void waitOnCond(cond_t* c, mutex_t* m, uint64_t usec) {
    #ifndef AUX_THREADING_STDC
    if (usec > 0) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += usec / 1000000;
        ts.tv_nsec += (usec % 1000000) * 1000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_nsec -= 1000000000;
            ts.tv_sec += 1;
        }
        pthread_cond_timedwait(c, m, &ts);
    } else {
        pthread_cond_wait(c, m);
    }
    #else
    if (usec > 0) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += usec / 1000000;
        ts.tv_nsec += (usec % 1000000) * 1000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_nsec -= 1000000000;
            ts.tv_sec += 1;
        }
        cnd_timedwait(c, m, &ts);
    } else {
        cnd_wait(c, m);
    }
    #endif
}

#endif
