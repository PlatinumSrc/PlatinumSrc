#include "version.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>

#include "glue.h"

#define _STR(x) #x
#define STR(x) _STR(x)

char* titlestr;
char verstr[128];
char platstr[128];
#if PLATFORM == PLAT_NXDK
char titleidstr[9];
#endif

void makeVerStrs(void) {
    titlestr = strdup("PlatinumSrc " STR(PSRC_BUILD));
    static const char* months[12] = {
        "Jan", "Feb", "Mar", "Apr",
        "May", "Jun", "Jul", "Aug",
        "Sep", "Oct", "Nov", "Dec"
    };
    snprintf(
        verstr, sizeof(verstr),
        "PlatinumSrc build %u (%s %u, %u; rev %u)",
        (unsigned)PSRC_BUILD,
        months[(((unsigned)PSRC_BUILD / 10000) % 100) - 1],
        ((unsigned)PSRC_BUILD / 100) % 100,
        (unsigned)PSRC_BUILD / 1000000,
        ((unsigned)PSRC_BUILD % 100) + 1
    );
    #if PLATFORM == PLAT_NXDK
    sprintf(titleidstr, "%08x", (unsigned)CURRENT_XBE_HEADER->CertificateHeader->TitleID);
    #endif
    snprintf(
        platstr, sizeof(platstr),
        "Platform: %s (Platform ID " STR(PLATFORM) "); Architecture: " ARCHSTR
        #if PLATFORM == PLAT_NXDK
        "; Title ID: %s"
        #endif
        , platname[PLATFORM]
        #if PLATFORM == PLAT_NXDK
        , titleidstr
        #endif
    );
}
