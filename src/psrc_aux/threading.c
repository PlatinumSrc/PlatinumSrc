#include "threading.h"
#include "logging.h"
#include "../debug.h"

#include <stddef.h>
#include <stdlib.h>

static void* threadwrapper(void* t_void) {
    thread_t* t = t_void;
    #ifndef AUX_THREADING_NONAMES
        #ifndef AUX_THREADING_STDC
            #if defined(__GLIBC__)
                pthread_setname_np(t->thread, t->name);
            #elif PLATFORM == PLAT_NETBSD
                pthread_setname_np(t->thread, "%s", t->name);
            #elif PLATFORM == PLAT_FREEBSD || PLATFORM == PLAT_OPENBSD
                pthread_set_name_np(t->thread, t->name);
            #elif PLATFORM == PLAT_MACOS
                pthread_setname_np(t->name);
            #endif
        #endif
    #endif
    return t->func(&t->data);
}

bool createThread(thread_t* t, const char* n, threadfunc_t f, void* a) {
    #if DEBUG(1)
    plog(LL_INFO, "Starting thread %s...", (n) ? n : "(null)");
    #endif
    t->name = (n) ? strdup(n) : NULL;
    t->func = f;
    t->data.args = a;
    t->data.shouldclose = false;
    bool fail;
    #ifndef AUX_THREADING_STDC
    fail = pthread_create(&t->thread, NULL, threadwrapper, t);
    #else
    fail = (thrd_create(&t->thread, (thrd_start_t)threadwrapper, t) != thrd_success);
    #endif
    if (fail) {
        free(t->name);
        return false;
    }
    return true;
}

void destroyThread(thread_t* t, void** r) {
    #if DEBUG(1)
    plog(LL_INFO, "Stopping thread %s...", (t->name) ? t->name : "(null)");
    #endif
    t->data.shouldclose = true;
    #ifndef AUX_THREADING_STDC
    pthread_join(t->thread, r);
    #else
    thrd_join(t->thread, (int*)r);
    #endif
    free(t->name);
}
