#ifndef PSRC_NOMT

#include "threading.h"
#include "logging.h"
#include "../debug.h"

#include <stddef.h>
#include <stdlib.h>

#if PLATFORM == PLAT_WIN32 && !defined(PSRC_UTILS_THREADING_WINPTHREAD)
DWORD WINAPI threadwrapper(LPVOID t) {
    ((thread_t*)t)->ret = ((thread_t*)t)->func(&((thread_t*)t)->data);
    ExitThread(0);
    return 0;
}
#else
static void* threadwrapper(void* t) {
    #ifndef UTILS_THREADING_NONAMES
        #ifndef PSRC_UTILS_THREADING_STDC
            #if defined(__GLIBC__)
                pthread_setname_np(((thread_t*)t)->thread, ((thread_t*)t)->name);
            #elif PLATFORM == PLAT_NETBSD
                pthread_setname_np(((thread_t*)t)->thread, "%s", ((thread_t*)t)->name);
            #elif PLATFORM == PLAT_FREEBSD || PLATFORM == PLAT_OPENBSD
                pthread_set_name_np(((thread_t*)t)->thread, ((thread_t*)t)->name);
            #elif PLATFORM == PLAT_MACOS
                pthread_setname_np(((thread_t*)t)->name);
            #endif
        #endif
    #endif
    return ((thread_t*)t)->func(&((thread_t*)t)->data);
}
#endif

bool createThread(thread_t* t, const char* n, threadfunc_t f, void* a) {
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG, "Starting thread %s...", (n) ? n : "(null)");
    #endif
    t->name = (n) ? strdup(n) : NULL;
    t->func = f;
    t->data.self = t;
    t->data.args = a;
    t->data.shouldclose = false;
    bool fail;
    #ifndef PSRC_UTILS_THREADING_STDC
    #if PLATFORM == PLAT_WIN32 && !defined(PSRC_UTILS_THREADING_WINPTHREAD)
    fail = !(t->thread = CreateThread(NULL, 0, threadwrapper, t, 0, NULL));
    #else
    fail = pthread_create(&t->thread, NULL, threadwrapper, t);
    #endif
    #else
    fail = (thrd_create(&t->thread, (thrd_start_t)threadwrapper, t) != thrd_success);
    #endif
    if (fail) {
        free(t->name);
        #if DEBUG(1)
        plog(LL_INFO | LF_DEBUG, "Failed to start thread %s", (n) ? n : "(null)");
        #endif
        return false;
    }
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG, "Started thread %s", (n) ? n : "(null)");
    #endif
    return true;
}

void quitThread(thread_t* t) {
    t->data.shouldclose = true;
}

void destroyThread(thread_t* t, void** r) {
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG, "Stopping thread %s...", (t->name) ? t->name : "(null)");
    #endif
    t->data.shouldclose = true;
    #ifndef PSRC_UTILS_THREADING_STDC
    #if PLATFORM == PLAT_WIN32 && !defined(PSRC_UTILS_THREADING_WINPTHREAD)
    WaitForSingleObject(t->thread, INFINITE);
    if (r) *r = t->ret;
    #else
    pthread_join(t->thread, r);
    #endif
    #else
    thrd_join(t->thread, (int*)r);
    #endif
    #if DEBUG(1)
    plog(LL_INFO | LF_DEBUG, "Stopped thread %s", (t->name) ? t->name : "(null)");
    #endif
    free(t->name);
}

#endif
