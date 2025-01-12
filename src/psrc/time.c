#include "time.h"

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#if PLATFORM == PLAT_NXDK
    #include <xboxkrnl/xboxkrnl.h>
#endif

#include "attribs.h"

#if PLATFORM == PLAT_NXDK
    uint64_t perfctfreq;
#elif (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    LARGE_INTEGER perfctfreq;
    uint64_t perfctmul = 1000000;
#endif

uint64_t altutime(void) {
    #if PLATFORM == PLAT_NXDK
        uint64_t time = KeQueryPerformanceCounter();
        return time * 1000000 / perfctfreq;
    #elif (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        LARGE_INTEGER time;
        QueryPerformanceCounter(&time);
        return time.QuadPart * perfctmul / perfctfreq.QuadPart;
    #else
        struct timespec time;
        clock_gettime(CLOCK_MONOTONIC, &time);
        return time.tv_sec * 1000000 + time.tv_nsec / 1000;
    #endif
}

void microwait(uint64_t d) {
    #if PLATFORM == PLAT_NXDK
        LARGE_INTEGER _d = {.QuadPart = d * -10};
        KeDelayExecutionThread(UserMode, true, &_d);
    #elif (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        #ifndef PSRC_NOMT
        static THREADLOCAL HANDLE timer = NULL;
        #else
        static HANDLE timer = NULL;
        #endif
        if (!timer) timer = CreateWaitableTimer(NULL, true, NULL);
        LARGE_INTEGER _d = {.QuadPart = d * -10};
        SetWaitableTimer(timer, &_d, 0, NULL, NULL, false);
        WaitForSingleObject(timer, INFINITE);
    #else
        struct timespec dts;
        dts.tv_sec = d / 1000000;
        dts.tv_nsec = (d % 1000000) * 1000;
        nanosleep(&dts, NULL);
    #endif
}
