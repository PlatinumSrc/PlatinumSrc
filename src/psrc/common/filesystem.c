#include "filesystem.h"
#include "string.h"
#include "logging.h"

#include <stddef.h>
#if PLATFORM == PLAT_NXDK
    #include <windows.h>
#elif PLATFORM == PLAT_DREAMCAST
    #include <dirent.h>
#else
    #include <sys/stat.h>
    #if PLATFORM == PLAT_WIN32
        #include <windows.h>
    #endif
    #ifndef PSRC_USESDL1
        #include <SDL2/SDL.h>
    #endif
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
    #if PLATFORM != PLAT_WIN32 && PLATFORM != PLAT_NXDK
    return (c == '/');
    #else
    return (c == '/' || c == '\\');
    #endif
}
static void replsep(struct charbuf* b, const char* s, bool first) {
    if (first) {
        #if PLATFORM != PLAT_WIN32 && PLATFORM != PLAT_NXDK
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
                #if PLATFORM != PLAT_WIN32 && PLATFORM != PLAT_NXDK
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
        #if PLATFORM != PLAT_WIN32 && PLATFORM != PLAT_NXDK
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

char* mkmaindir(void) {
    #if PLATFORM == PLAT_NXDK
    return strdup("D:\\");
    #elif PLATFORM == PLAT_DREAMCAST
    DIR* d = opendir("/cd");
    if (d) {
        closedir(d);
        return strdup("/cd");
    } else {
        return strdup("/sd/psrc");
    }
    #elif PLATFORM == PLAT_3DS
    return strdup("sdmc:/3ds/psrc");
    #elif PLATFORM == PLAT_WII
    return strdup("/apps/psrc");
    #elif PLATFORM == PLAT_GAMECUBE
    return strdup("/");
    #elif !defined(PSRC_USESDL1)
    char* tmp = SDL_GetBasePath();
    if (tmp) {
        char* tmp2 = tmp;
        tmp = mkpath(tmp, NULL);
        SDL_free(tmp2);
        return tmp;
    } else {
        plog(LL_WARN, "Failed to get main directory: %s", SDL_GetError());
        return strdup(".");
    }
    #elif PLATFORM == PLAT_WIN32
    char* tmp = malloc(MAX_PATH + 1);
    tmp[MAX_PATH] = 0;
    DWORD r = GetModuleFileName(NULL, s, MAX_PATH);
    if (r) {
        for (int i = r - 1; i > 0; --i) {
            if (tmp[r] == '\\' || tmp[r] == '/') {
                tmp[r] = 0;
                tmp = realloc(tmp, r + 1);
                break;
            }
        }
        return tmp;
    } else {
        free(tmp);
        plog(LL_WARN, "Failed to get main directory");
        return strdup(".");
    }
    #else // TODO: implement more platforms
    plog(LL_WARN, "Failed to get main directory");
    return strdup(".");
    #endif
}
char* mkuserdir(const char* m, const char* n) {
    #if PLATFORM == PLAT_NXDK
    (void)m;
    uint32_t titleid = CURRENT_XBE_HEADER->CertificateHeader->TitleID;
    char titleidstr[9];
    sprintf(titleidstr, "%08x", (unsigned)titleid);
    return mkpath("E:\\TDATA", titleidstr, "data", n, NULL);
    #elif PLATFORM == PLAT_DREAMCAST
    (void)m;
    return mkpath("/rd", n, NULL);
    #elif PLATFORM == PLAT_3DS || PLATFORM == PLAT_WII || PLATFORM == PLAT_GAMECUBE
    return mkpath(m, "data", n, NULL);
    #elif !defined(PSRC_USESDL1)
    (void)m;
    char* tmp = SDL_GetPrefPath(NULL, n);
    if (tmp) {
        char* tmp2 = tmp;
        tmp = mkpath(tmp, NULL);
        SDL_free(tmp2);
        return tmp;
    } else {
        plog(LL_WARN, "Failed to get user directory: %s", SDL_GetError());
        return mkpath(m, "data", n, NULL);
    }
    #elif PLATFORM == PLAT_WIN32
    char* tmp = getenv("AppData");
    if (tmp) {
        return mkpath(tmp, n, NULL);
    } else {
        plog(LL_WARN, LP_WARN "Failed to get user directory");
        return mkpath(m, "data", n, NULL);
    }
    #else
    plog(LL_WARN, LP_WARN "Failed to get user directory");
    return mkpath(m, "data", n, NULL);
    #endif
}
