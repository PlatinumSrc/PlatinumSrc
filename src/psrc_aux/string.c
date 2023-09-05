#include "string.h"
#include "../platform.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// inefficient but it works
char* strcombine(const char* s, ...) {
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

char** splitstrlist(const char* s, char delim, bool nullterm, int* l) {
    int len = 0;
    int size = 4;
    char** data = malloc(size * sizeof(*data));
    struct charbuf tmpcb;
    cb_init(&tmpcb, 256);
    char c;
    while (1) {
        c = *s;
        if (c == '\\') {
            if ((c = *++s)) {
                if (c == delim) {
                    cb_add(&tmpcb, delim);
                } else if (c == '\\') {
                    cb_add(&tmpcb, '\\');
                } else {
                    cb_add(&tmpcb, '\\');
                    cb_add(&tmpcb, c);
                }
            } else {
                cb_add(&tmpcb, '\\');
                break;
            }
        } else if (c == delim || !c) {
            ++len;
            if (len == size) {
                size *= 2;
                data = realloc(data, size * sizeof(*data));
            }
            data[len - 1] = cb_reinit(&tmpcb, 256);
            if (!c) break;
        } else {
            cb_add(&tmpcb, c);
        }
        ++s;
    }
    if (nullterm) {
        ++len;
        if (len != size) {
            data = realloc(data, len * sizeof(*data));
        }
        --len;
        data[len] = NULL;
    } else {
        if (len != size) {
            data = realloc(data, len * sizeof(*data));
        }
    }
    if (l) *l = len;
    return data;
}

char* makestrlist(const char* const* s, int l, char delim) {
    if (!l) return calloc(1, 1);
    const char* tmp = *s;
    if (!tmp) return calloc(1, 1);
    struct charbuf cb;
    cb_init(&cb, 256);
    while (1) {
        char c;
        while ((c = *tmp)) {
            if (c == delim || c == '\\') cb_add(&cb, '\\');
            cb_add(&cb, c);
            ++tmp;
        }
        tmp = *++s;
        if (!tmp) break;
        if (l > 1) --l;
        else break;
        cb_add(&cb, delim);
    }
    return cb_finalize(&cb);
}
