#include "fs.h"
#include "string.h"
#include "../platform.h"

#include <stddef.h>
#if PLATFORM != PLAT_XBOX
    #include <sys/stat.h>
#else
    //#include <dirent.h>
#endif
#include <stdio.h>
#include <stdbool.h>

int isFile(const char* p) {
    #if PLATFORM != PLAT_XBOX
        struct stat s;
        if (stat(p, &s)) return -1;
        if (S_ISREG(s.st_mode)) return 1;
        if (S_ISDIR(s.st_mode)) return 0;
        return 2;
    #else
        //DIR* d = opendir(p);
        //if (!d) return 0;
        //closedir(d);
        FILE* f = fopen(p, "r");
        if (!f) return -1;
        fclose(f);
        return 1;
    #endif
}

long getFileSize(FILE* f, bool c) {
    if (!f) return -1;
    long ret = 0;
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

static long trimsep(const char* s) {
    long len = 0, sub = 0;
    char c;
    #if PLATFORM != PLAT_WINDOWS && PLATFORM != PLAT_XBOX
    // check for /, //, ///, etc and return /
    // end check if it turns out to be a dir in /
    while (1) {
        c = s[len];
        if (c != '/') {
            if (c) {
                break;
            } else {
                return 1;
            }
        }
        ++len;
    }
    #endif
    while (1) {
        if (!(c = s[len])) break;
        #if PLATFORM != PLAT_WINDOWS && PLATFORM != PLAT_XBOX
        if (c == '/') {
            ++sub;
        } else {
            sub = 0;
        }
        #else
        if (c == '/' || c == '\\') {
            ++sub;
        } else {
            sub = 0;
        }
        #endif
        ++len;
    }
    return len - sub;
}
char* mkpath(const char* s, ...) {
    if (!s) return NULL;
    struct charbuf b;
    cb_init(&b, 256);
    cb_addpartstr(&b, s, trimsep(s));
    va_list v;
    va_start(v, s);
    s = va_arg(v, char*);
    while (s) {
        #if PLATFORM != PLAT_WINDOWS && PLATFORM != PLAT_XBOX
        cb_add(&b, '/');
        #else
        cb_add(&b, '\\');
        #endif
        cb_addpartstr(&b, s, trimsep(s));
        s = va_arg(v, char*);
    }
    va_end(v);
    return cb_finalize(&b);
}
