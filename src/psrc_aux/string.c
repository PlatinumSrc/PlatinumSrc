#include "string.h"
#include "../platform.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// inefficient but it works
char* strcombine(char* s, ...) {
    struct charbuf b;
    cb_init(&b, 256);
    va_list v;
    va_start(v, s);
    while (s) {
        cb_addstr(&b, s);
        s = va_arg(v, char*);
    }
    va_end(v);
    return cb_finalize(&b);
}
