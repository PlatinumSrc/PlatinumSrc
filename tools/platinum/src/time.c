#include "time.h"

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
LARGE_INTEGER perfctfreq;
uint64_t perfctmul = 1000000;
#endif

uint64_t altutime(void) {
    #ifndef _WIN32
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000000 + time.tv_nsec / 1000;
    #else
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time.QuadPart * perfctmul / perfctfreq.QuadPart;
    #endif
}

void microwait(uint64_t d) {
    #ifndef _WIN32
    struct timespec dts;
    dts.tv_sec = d / 1000000;
    dts.tv_nsec = (d % 1000000) * 1000;
    nanosleep(&dts, NULL);
    #else
    static HANDLE timer = NULL;
    if (!timer) timer = CreateWaitableTimer(NULL, true, NULL);
    LARGE_INTEGER _d = {.QuadPart = d * -10};
    SetWaitableTimer(timer, &_d, 0, NULL, NULL, false);
    WaitForSingleObject(timer, INFINITE);
    #endif
}
