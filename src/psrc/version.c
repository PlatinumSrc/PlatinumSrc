#include "version.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>

#define _STR(x) #x
#define STR(x) _STR(x)

char* titlestr;
char verstr[64];
char platstr[64];

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
    snprintf(
        platstr, sizeof(platstr),
        "Platform: %s (Platform ID " STR(PLATFORM) "); Architecture: " ARCHSTR,
        platname[PLATFORM]
    );
}
