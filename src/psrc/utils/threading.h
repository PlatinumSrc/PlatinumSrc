#ifndef PSRC_UTILS_THREADING_H
#define PSRC_UTILS_THREADING_H

#include "../platform.h"

#include <stdbool.h>
#include <stdint.h>

#ifndef UTILS_THREADING_STDC
    #include <pthread.h>
#else
    #include <threads.h>
#endif

struct thread_t;
struct thread_data {
    struct thread_t* self;
    void* args;
    volatile bool shouldclose;
};
typedef void* (*threadfunc_t)(struct thread_data*);
typedef struct thread_t {
    #ifndef UTILS_THREADING_STDC
    pthread_t thread;
    #else
    thrd_t thread;
    #endif
    char* name;
    threadfunc_t func;
    struct thread_data data;
} thread_t;
#ifndef UTILS_THREADING_STDC
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;
#else
typedef mtx_t mutex_t;
typedef cnd_t cond_t;
#endif
struct accesslock {
    int counter;
    mutex_t lock;
};

bool createThread(thread_t*, const char* name, threadfunc_t func, void* args);
void quitThread(thread_t*);
void destroyThread(thread_t*, void** ret);

#ifndef UTILS_THREADING_STDC
    #define yield() sched_yield()
#else
    #define yield() thrd_yield()
#endif

static inline bool createMutex(mutex_t* m) {
    #ifndef UTILS_THREADING_STDC
    return !pthread_mutex_init(m, NULL);
    #else
    return (mtx_init(m, mtx_plain) == thrd_success);
    #endif
}
static inline void lockMutex(mutex_t* m) {
    #ifndef UTILS_THREADING_STDC
    while (pthread_mutex_lock(m)) {}
    #else
    #if PLATFORM != PLAT_NXDK
    while (mtx_lock(m) != thrd_success) {}
    #else
    // Ignore return value on Xbox. The NXDK doesn't check the return code of NtWaitForSingleObject correctly (mtx_lock
    // checks for STATUS_WAIT_0 instead of using NT_SUCCESS()).
    mtx_lock(m);
    #endif
    #endif
}
static inline void unlockMutex(mutex_t* m) {
    #ifndef UTILS_THREADING_STDC
    while (pthread_mutex_unlock(m)) {}
    #else
    while (mtx_unlock(m) != thrd_success) {}
    #endif
}
static inline void destroyMutex(mutex_t* m) {
    #ifndef UTILS_THREADING_STDC
    pthread_mutex_destroy(m);
    #else
    mtx_destroy(m);
    #endif
}

static inline bool createCond(cond_t* c) {
    #ifndef UTILS_THREADING_STDC
    return !pthread_cond_init(c, NULL);
    #else
    return (cnd_init(c) == thrd_success);
    #endif
}
static inline void waitOnCond(cond_t* c, mutex_t* m, uint64_t t) {
    if (t) return; // TODO: imeplement later
    #ifndef UTILS_THREADING_STDC
    pthread_cond_wait(c, m);
    #else
    cnd_wait(c, m);
    #endif
}
static inline void signalCond(cond_t* c) {
    #ifndef UTILS_THREADING_STDC
    pthread_cond_signal(c);
    #else
    cnd_signal(c);
    #endif
}
static inline void broadcastCond(cond_t* c) {
    #ifndef UTILS_THREADING_STDC
    pthread_cond_broadcast(c);
    #else
    cnd_broadcast(c);
    #endif
}
static inline void destroyCond(cond_t* c) {
    #ifndef UTILS_THREADING_STDC
    pthread_cond_destroy(c);
    #else
    cnd_destroy(c);
    #endif
}

static inline bool createAccessLock(struct accesslock* a) {
    if (!createMutex(&a->lock)) return false;
    a->counter = 0;
    return true;
}
static inline void destroyAccessLock(struct accesslock* a) {
    lockMutex(&a->lock);
    while (a->counter) {
        unlockMutex(&a->lock);
        yield();
        lockMutex(&a->lock);
    }
    destroyMutex(&a->lock);
}
static inline void acquireReadAccess(struct accesslock* a) {
    lockMutex(&a->lock);
    ++a->counter;
    unlockMutex(&a->lock);
}
static inline void releaseReadAccess(struct accesslock* a) {
    lockMutex(&a->lock);
    --a->counter;
    unlockMutex(&a->lock);
}
static inline void acquireWriteAccess(struct accesslock* a) {
    lockMutex(&a->lock);
    while (a->counter) {
        unlockMutex(&a->lock);
        yield();
        lockMutex(&a->lock);
    }
}
static inline void releaseWriteAccess(struct accesslock* a) {
    unlockMutex(&a->lock);
}
static inline void readToWriteAccess(struct accesslock* a) {
    lockMutex(&a->lock);
    --a->counter;
    while (a->counter) {
        unlockMutex(&a->lock);
        yield();
        lockMutex(&a->lock);
    }
}
static inline void writeToReadAccess(struct accesslock* a) {
    ++a->counter;
    unlockMutex(&a->lock);
}
static inline void yieldReadAccess(struct accesslock* a) {
    lockMutex(&a->lock);
    --a->counter;
    unlockMutex(&a->lock);
    yield();
    lockMutex(&a->lock);
    ++a->counter;
    unlockMutex(&a->lock);
}

#endif
