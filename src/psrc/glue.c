#include "glue.h"
#include "platform.h"

#if PLATFORM == PLAT_NXDK
// objcopy refuses to put this section in the right place
__asm__ (
    ".section \"XTIMAGE\"\n"
    ".byte 0"
);
#endif
