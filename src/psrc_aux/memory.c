#include "memory.h"

#include "../platform.h"

#if PLATFORM == PLAT_WINDOWS
    #include <windows.h>
#elif PLATFORM == PLAT_XBOX
    #include <xboxkrnl/xboxkrnl.h>
#else
    #include <unistd.h>
#endif
#include <stdio.h>

static inline void getMemStats_internal(uintptr_t* t, uintptr_t* a) {
    #if PLATFORM == PLAT_WINDOWS
        MEMORYSTATUSEX mstats = {.dwLength = sizeof(mstats)};
        GlobalMemoryStatusEx(&mstats);
        if (t) *t = mstats.ullTotalPhys;
        if (a) *a = mstats.ullAvailPhys;
    #elif PLATFORM == PLAT_XBOX
        MM_STATISTICS mstats = {.Length = sizeof(mstats)};
        MmQueryStatistics(&mstats);
        if (t) *t = mstats.TotalPhysicalPages * PAGE_SIZE;
        if (a) *a = mstats.AvailablePages * PAGE_SIZE;
    #else
        uintptr_t pgsz = sysconf(_SC_PAGESIZE);
        if (t) *t = (sysconf(_SC_PHYS_PAGES) * pgsz);
        if (a) *a = (sysconf(_SC_AVPHYS_PAGES) * pgsz);
    #endif
}

void getMemStats(uintptr_t* t, uintptr_t* u) {
    getMemStats_internal(t, u);
    if (u) *u = *t - *u;
}

void getMemStatString(char* b, size_t sz) {
    uintptr_t mtotal;
    uintptr_t mavail;
    getMemStats_internal(&mtotal, &mavail);
    uintptr_t mused = mtotal - mavail;
    int usage = ((uint64_t)mused * 100) / (uint64_t)mtotal;
    mtotal = (uint64_t)mtotal * 1000 / 1024 * 1000 / 1024;
    mavail = (uint64_t)mavail * 1000 / 1024 * 1000 / 1024;
    mused = (uint64_t)mused * 1000 / 1024 * 1000 / 1024;
    uintptr_t mtotal_dec = (mtotal % 1000000) / 1000;
    uintptr_t mavail_dec = (mavail % 1000000) / 1000;
    uintptr_t mused_dec = (mused % 1000000) / 1000;
    int mtotal_pad = 3;
    int mavail_pad = 3;
    int mused_pad = 3;
    while (mtotal_pad > 1 && mtotal_dec % 10 == 0) {mtotal_dec /= 10; --mtotal_pad;}
    while (mavail_pad > 1 && mavail_dec % 10 == 0) {mavail_dec /= 10; --mavail_pad;}
    while (mused_pad > 1 && mused_dec % 10 == 0) {mused_dec /= 10; --mused_pad;}
    mtotal /= 1000000;
    mavail /= 1000000;
    mused /= 1000000;
    snprintf(
        b, sz,
        "%lu.%0*luMiB used (%lu.%0*luMiB available) out of %lu.%0*luMiB (%d%% used)",
        mused, mused_pad, mused_dec,
        mavail, mavail_pad, mavail_dec,
        mtotal, mtotal_pad, mtotal_dec,
        usage
    );
}

int getMemUsage() {
    #if PLATFORM != PLAT_WINDOWS
        //uint64_t u
    #else
        MEMORYSTATUSEX mstats;
        mstats.dwLength = sizeof(mstats);
        GlobalMemoryStatusEx(&mstats);
        return mstats.dwMemoryLoad;
    #endif
}
