#include "glue.h"
#include "platform.h"

#if PLATFORM == PLAT_NXDK
    #include <ctype.h>
#endif
#if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #include "filesystem.h"
#endif

#if PLATFORM == PLAT_NXDK
// objcopy refuses to put this section in the right place
__asm__ (
    ".pushsection \"XTIMAGE\"\n"
    ".byte 0\n"
    ".popsection\n"
);

// the nxdk is missing atof
double atof(const char *s) {
    double a = 0.0;
    int e = 0;
    int c;
    while ((c = *s++) != '\0' && isdigit(c)) {
        a = a * 10.0 + (c - '0');
    }
    if (c == '.') {
        while ((c = *s++) != '\0' && isdigit(c)) {
            a = a * 10.0 + (c - '0');
            e = e - 1;
        }
    }
    if (c == 'e' || c == 'E') {
        int sign = 1;
        int i = 0;
        c = *s++;
        if (c == '+') {
            c = *s++;
        } else if (c == '-') {
            c = *s++;
            sign = -1;
        }
        while (isdigit(c)) {
            i = i * 10 + (c - '0');
            c = *s++;
        }
        e += i * sign;
    }
    while (e > 0) {
        a *= 10.0;
        --e;
    }
    while (e < 0) {
        a *= 0.1;
        ++e;
    }
    return a;
}
#endif
