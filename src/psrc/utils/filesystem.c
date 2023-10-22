#include "filesystem.h"
#include "string.h"

#include <stddef.h>
#if PLATFORM != PLAT_NXDK
    #include <sys/stat.h>
#else
    #include <windows.h>
#endif
#include <stdio.h>
#include <stdbool.h>

int isFile(const char* p) {
    #if PLATFORM != PLAT_NXDK
        struct stat s;
        if (stat(p, &s)) return -1;
        if (S_ISREG(s.st_mode)) return 1;
        if (S_ISDIR(s.st_mode)) return 0;
        return 2;
    #else
        DWORD a = GetFileAttributes(p);
        if (a == INVALID_FILE_ATTRIBUTES) return -1;
        if (a & FILE_ATTRIBUTE_DIRECTORY) return 0;
        if (a & FILE_ATTRIBUTE_DEVICE) return 2;
        return 1;
    #endif
}

long getFileSize(FILE* f, bool c) {
    if (!f) return -1;
    long ret = -1;
    long tmp;
    if (!c) tmp = ftell(f);
    if (!fseek(f, 0, SEEK_END)) ret = ftell(f);
    if (c) {
        fclose(f);
    } else {
        fseek(f, tmp, SEEK_SET);
    }
    return ret;
}

static inline bool isSepChar(char c) {
    #if PLATFORM != PLAT_WINDOWS && PLATFORM != PLAT_NXDK
    return (c == '/');
    #else
    return (c == '/' || c == '\\');
    #endif
}
static void replsep(struct charbuf* b, const char* s, bool first) {
    if (first) {
        #if PLATFORM != PLAT_WINDOWS && PLATFORM != PLAT_NXDK
        if (*s == '/') {
            cb_add(b, '/');
            ++s;
        }
        #endif
    }
    while (isSepChar(*s)) ++s;
    bool sep = false;
    while (1) {
        char c = *s;
        if (!c) break;
        ++s;
        if (isSepChar(c)) {
            sep = true;
        } else {
            if (sep) {
                #if PLATFORM != PLAT_WINDOWS && PLATFORM != PLAT_NXDK
                cb_add(b, '/');
                #else
                cb_add(b, '\\');
                #endif
                sep = false;
            }
            cb_add(b, c);
        }
    }
}
char* mkpath(const char* s, ...) {
    struct charbuf b;
    cb_init(&b, 256);
    if (s) {
        replsep(&b, s, true);
    }
    va_list v;
    va_start(v, s);
    if (!s) {
        if ((s = va_arg(v, char*))) {
            replsep(&b, s, false);
        } else {
            goto ret;
        }
    }
    while ((s = va_arg(v, char*))) {
        #if PLATFORM != PLAT_WINDOWS && PLATFORM != PLAT_NXDK
        cb_add(&b, '/');
        #else
        cb_add(&b, '\\');
        #endif
        replsep(&b, s, false);
    }
    ret:;
    va_end(v);
    return cb_finalize(&b);
}
char* strpath(const char* s) {
    struct charbuf b;
    cb_init(&b, 256);
    replsep(&b, s, true);
    return cb_finalize(&b);
}
char* strrelpath(const char* s) {
    struct charbuf b;
    cb_init(&b, 256);
    replsep(&b, s, false);
    return cb_finalize(&b);
}
