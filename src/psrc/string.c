#include "string.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "glue.h"

char** splitstrlist(const char* s, char delim, bool nullterm, size_t* l) {
    size_t len = 0;
    size_t size = 4;
    char** data = malloc(size * sizeof(*data));
    if (!data) return NULL;
    struct charbuf tmpcb;
    if (!cb_init(&tmpcb, 256)) return false;
    char c;
    size_t ol = 0;
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
                char** tmpptr = realloc(data, size * sizeof(*data));
                if (!tmpptr) {
                    cb_dump(&tmpcb);
                    free(data);
                    return NULL;
                }
                data = tmpptr;
            }
            data[len++] = (char*)ol;
            if (!c) break;
            cb_add(&tmpcb, 0);
            ol = tmpcb.len;
        } else {
            cb_add(&tmpcb, c);
        }
        ++s;
    }
    cb_finalize(&tmpcb);
    if (nullterm) {
        ++len;
        if (len != size) {
            char** tmpptr = realloc(data, len * sizeof(*data));
            if (!tmpptr) {
                cb_dump(&tmpcb);
                free(data);
                return NULL;
            }
            data = tmpptr;
        }
        --len;
        data[len] = NULL;
    } else {
        if (len != size) {
            char** tmpptr = realloc(data, len * sizeof(*data));
            if (tmpptr) data = tmpptr;
        }
    }
    for (size_t i = 0; i < len; ++i) {
        data[i] += (uintptr_t)tmpcb.data;
    }
    if (l) *l = len;
    return data;
}
char** splitstr(const char* s, const char* delims, bool nullterm, size_t* l) {
    size_t len = 0;
    size_t size = 4;
    char** data = malloc(size * sizeof(*data));
    if (!data) return NULL;
    struct charbuf tmpcb;
    if (!cb_init(&tmpcb, 256)) return NULL;
    size_t ol = 0;
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
                char** tmpptr = realloc(data, size * sizeof(*data));
                if (!tmpptr) {
                    cb_dump(&tmpcb);
                    free(data);
                    return NULL;
                }
                data = tmpptr;
            }
            data[len++] = (char*)ol;
            if (!c) break;
            cb_add(&tmpcb, 0);
            ol = tmpcb.len;
            split = false;
        } else {
            cb_add(&tmpcb, c);
        }
        ++s;
    }
    cb_finalize(&tmpcb);
    if (nullterm) {
        ++len;
        if (len != size) {
            char** tmpptr = realloc(data, len * sizeof(*data));
            if (!tmpptr) {
                cb_dump(&tmpcb);
                free(data);
                return NULL;
            }
            data = tmpptr;
        }
        --len;
        data[len] = NULL;
    } else {
        if (len != size) {
            char** tmpptr = realloc(data, len * sizeof(*data));
            if (tmpptr) data = tmpptr;
        }
    }
    for (size_t i = 0; i < len; ++i) {
        data[i] += (uintptr_t)tmpcb.data;
    }
    if (l) *l = len;
    return data;
}

char* makestrlist(const char* const* s, size_t l, char delim) {
    if (!l) return NULL;
    const char* tmp = *s;
    if (!tmp) return NULL;
    struct charbuf cb;
    if (!cb_init(&cb, 256)) return NULL;
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

uint64_t strsec(const char* s, uint64_t d) {
    uint64_t n = 0;
    char c = *s;
    if (!c) return d;
    do {
        if (c == '.') {
            ++s;
            uint64_t m = 100000;
            while ((c = *s)) {
                if (c < '0' || c > '9') return d;
                n += (c - '0') * m;
                m /= 10;
                ++s;
            }
            return n;
        }
        if (c < '0' || c > '9') return d;
        n *= 10;
        n += (c - '0') * 1000000;
        ++s;
    } while ((c = *s));
    return n;
}
