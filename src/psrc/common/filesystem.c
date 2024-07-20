#include "filesystem.h"
#include "string.h"
#include "logging.h"

#include <stddef.h>
#if PLATFORM == PLAT_NXDK
    #include <windows.h>
#elif PLATFORM == PLAT_DREAMCAST
    #include <dirent.h>
#else
    #include <sys/types.h>
    #include <sys/stat.h>
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        #include <dirent.h>
        #include <unistd.h>
    #else
        #include <windows.h>
    #endif
    #ifndef PSRC_USESDL1
        #if PLATFORM == PLAT_NXDK || PLATFORM == PLAT_GDK
            #include <SDL.h>
        #else
            #include <SDL2/SDL.h>
        #endif
    #endif
#endif
#include <stdio.h>
#include <stdbool.h>

#include "../glue.h"
#include "../attribs.h"

int isFile(const char* p) {
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
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
    if (c) fclose(f);
    else fseek(f, tmp, SEEK_SET);
    return ret;
}

static ALWAYSINLINE bool isSepChar(char c) {
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    return (c == '/');
    #else
    return (c == '/' || c == '\\');
    #endif
}

static void replsep(struct charbuf* b, const char* s, bool first) {
    if (first) {
        #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
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
                cb_add(b, PATHSEP);
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
        cb_add(&b, PATHSEP);
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

bool md(const char* p) {
    struct charbuf cb;
    cb_init(&cb, 256);
    char c = *p;
    if (isSepChar(c)) {cb_add(&cb, c); ++p;}
    while ((c == *p)) {
        if (isSepChar(c)) {
            int t = isFile(cb_peek(&cb));
            if (t < 0) {
                bool cond;
                #if PLATFORM != PLAT_NXDK
                cond = (mkdir(cb.data) < 0);
                #else
                cond = (!CreateDirectory(cb.data, NULL));
                #endif
                if (cond) {
                    cb_dump(&cb);
                    return false;
                }
            } else if (t > 0) {
                // is a file
                cb_dump(&cb);
                return false;
            }
        }
        cb_add(&cb, c);
        ++p;
    }
    cb_dump(&cb);
    return true;
}

char** ls(const char* p, bool ln, int* l) {
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    DIR* d = opendir(p);
    if (!d) return NULL;
    #else
    #endif
    struct charbuf names, statname;
    cb_init(&names, 256);
    cb_init(&statname, 256);
    cb_addstr(&statname, p);
    if (statname.len > 0 && !isSepChar(statname.data[statname.len - 1])) cb_add(&statname, PATHSEP);
    int snl = statname.len;
    char** data = malloc(16 * sizeof(*data));
    int len = 1;
    int size = 16;
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    struct dirent* de;
    while ((de = readdir(d))) {
        char* n = de->d_name;
        if (n[0] == '.' && (!n[1] || (n[1] == '.' && !n[2]))) continue; // skip . and ..
        cb_addstr(&statname, n);
        struct stat s;
        if (!stat(cb_peek(&statname), &s)) {
            char i = (S_ISDIR(s.st_mode)) ? LS_ISDIR : 0;
            if (S_ISCHR(s.st_mode)) i |= LS_ISSPECIAL;
            else if (S_ISBLK(s.st_mode)) i |= LS_ISSPECIAL;
            else if (S_ISFIFO(s.st_mode)) i |= LS_ISSPECIAL;
            #ifdef S_ISSOCK
            else if (S_ISSOCK(s.st_mode)) i |= LS_ISSPECIAL;
            #endif
            #ifdef S_ISLNK
            lstat(statname.data, &s);
            if (S_ISLNK(s.st_mode)) i |= LS_ISLNK;
            #endif
            cb_add(&names, i);
            if (len == size) {
                size *= 2;
                data = realloc(data, size * sizeof(*data));
            }
            data[len++] = (char*)(uintptr_t)names.len;
            cb_addstr(&names, (ln) ? statname.data : n);
            cb_add(&names, 0);
        }
        statname.len = snl;
    }
    closedir(d);
    #else
    #endif
    cb_dump(&statname);
    names.data = realloc(names.data, names.len);
    data = realloc(data, (len + 1) * sizeof(*data));
    *data = names.data;
    data[len] = NULL;
    --len;
    ++data;
    for (int i = 0; i < len; ++i) {
        data[i] = (char*)(((uintptr_t)data[i]) + (uintptr_t)names.data);
    }
    if (l) *l = len;
    return data;
}
void freels(char** d) {
    --d;
    free(*d);
    free(d);
}

bool rm(const char* p) {
    
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
