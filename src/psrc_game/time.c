#include "time.h"

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#if PLATFORM == PLAT_XBOX
    #include <xboxkrnl/xboxkrnl.h>
#endif

#if PLATFORM == PLAT_WINDOWS || PLATFORM == PLAT_XBOX
LARGE_INTEGER perfctfreq = {.QuadPart = 100};
#endif

uint64_t altutime(void) {
    #if PLATFORM == PLAT_WINDOWS || PLATFORM == PLAT_XBOX
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time.QuadPart * 1000000 / perfctfreq.QuadPart;
    #else
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000000 + time.tv_nsec / 1000;
    #endif
}

void microwait(uint64_t d) {
    #if PLATFORM == PLAT_WINDOWS
    HANDLE timer = CreateWaitableTimer(NULL, true, NULL);
    LARGE_INTEGER _d = {.QuadPart = d * -10};
    SetWaitableTimer(timer, &_d, 0, NULL, NULL, false);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
    #elif PLATFORM == PLAT_XBOX
    LARGE_INTEGER _d = {.QuadPart = d * -10};
    KeDelayExecutionThread(UserMode, true, &_d);
    #else
    struct timespec dts;
    dts.tv_sec = d / 1000000;
    dts.tv_nsec = (d % 1000000) * 1000;
    nanosleep(&dts, NULL);
    #endif
}
