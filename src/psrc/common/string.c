#include "string.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "../glue.h"

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
    int ol = 0;
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
            if (len == size) {
                size *= 2;
                data = realloc(data, size * sizeof(*data));
            }
            data[len++] = (char*)(uintptr_t)ol;
            if (!c) break;
            cb_add(&tmpcb, 0);
            ol = tmpcb.len;
        } else {
            cb_add(&tmpcb, c);
        }
        ++s;
    }
    cb_nullterm(&tmpcb);
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
    for (int i = 0; i < len; ++i) {
        data[i] += (uintptr_t)tmpcb.data;
    }
    if (l) *l = len;
    return data;
}
char** splitstr(const char* s, const char* delims, bool nullterm, int* l) {
    int len = 0;
    int size = 4;
    char** data = malloc(size * sizeof(*data));
    struct charbuf tmpcb;
    cb_init(&tmpcb, 256);
    int ol = 0;
    char c;
    bool split = false;
    while (1) {
        c = *s;
        if (!c) {
            split = true;
        } else {
            for (const char* d = delims; *d; ++d) {
                if (c == *d) {split = true; break;}
            }
        }
        if (split) {
            if (len == size) {
                size *= 2;
                data = realloc(data, size * sizeof(*data));
            }
            data[len++] = (char*)(uintptr_t)ol;
            if (!c) break;
            cb_add(&tmpcb, 0);
            ol = tmpcb.len;
            split = false;
        } else {
            cb_add(&tmpcb, c);
        }
        ++s;
    }
    cb_nullterm(&tmpcb);
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
    for (int i = 0; i < len; ++i) {
        data[i] += (uintptr_t)tmpcb.data;
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

int strbool(const char* s, int d) {
    if (!s) return d;
    if (!strcasecmp(s, "true") || !strcasecmp(s, "yes") || !strcasecmp(s, "y") || !strcasecmp(s, "on")) {
        return 1;
    } else if (!strcasecmp(s, "false") || !strcasecmp(s, "no") || !strcasecmp(s, "n") || !strcasecmp(s, "off")) {
        return 0;
    }
    return d;
}
