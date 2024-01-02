#ifndef PSRC_ENGINEMAIN_MAIN_H
#define PSRC_ENGINEMAIN_MAIN_H

#include "../platform.h"

#if PLATFORM == PLAT_NXDK
#ifndef PSRC_NOMT
void armWatchdog(unsigned);
void cancelWatchdog(void);
void rearmWatchdog(unsigned);
#else
#define armWatchdog(x) ((void)(x))
#define cancelWatchdog()
#define rearmWatchdog(x) ((void)(x))
#endif
#endif

#endif
