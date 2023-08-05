#ifndef AUX_THREADING_H
#define AUX_THREADING_H

#include "../platform.h"

#include <stdbool.h>

#if PLATFORM == PLAT_XBOX
    #define AUX_THREADING_STDC
#endif
#ifndef AUX_THREADING_STDC
    #include <pthread.h>
#else
    #include <threads.h>
#endif

struct thread_data {
    void* args;
    bool shouldclose;
};
typedef void* (*threadfunc_t)(struct thread_data*);
typedef struct {
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
#else
typedef mtx_t mutex_t;
#endif

bool createThread(thread_t* thread, const char* name, threadfunc_t func, void* args);
void destroyThread(thread_t* thread, void** ret);

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

#endif
