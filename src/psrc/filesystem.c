#include "filesystem.h"
#include "string.h"
#include "logging.h"

#include <stddef.h>
#include <ctype.h>
#if PLATFORM != PLAT_NXDK
    #include <errno.h>
    #if PLATFORM == PLAT_DREAMCAST
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
#else
    #include <windows.h>
#endif
#include <stdio.h>
#include <stdbool.h>

#include "glue.h"

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

char* basepathname(char* p) {
    int i = 0;
    char* r = p;
    while (1) {
        int o = i;
        while (ispathsep(p[i])) {
            ++i;
        }
        if (!p[i]) {
            p[o] = 0;
            return r;
        }
        r = &p[i];
        ++i;
        while (!ispathsep(p[i])) {
            if (!p[i]) return r;
            ++i;
        }
    }
}

void replpathsep(struct charbuf* b, const char* s, bool first) {
    if (first) {
        #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        if (*s == '/') {
            cb_add(b, '/');
            ++s;
        }
        #endif
    }
    while (ispathsep(*s)) ++s;
    bool sep = false;
    while (1) {
        char c = *s;
        if (!c) break;
        ++s;
        if (ispathsep(c)) {
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
        replpathsep(&b, s, true);
    }
    va_list v;
    va_start(v, s);
    if (!s) {
        if ((s = va_arg(v, char*))) {
            replpathsep(&b, s, false);
        } else {
            goto ret;
        }
    }
    while ((s = va_arg(v, char*))) {
        if (b.len && b.data[b.len - 1] != PATHSEP) cb_add(&b, PATHSEP);
        replpathsep(&b, s, false);
    }
    ret:;
    va_end(v);
    return cb_finalize(&b);
}
char* strpath(const char* s) {
    struct charbuf b;
    cb_init(&b, 256);
    replpathsep(&b, s, true);
    return cb_finalize(&b);
}
char* strrelpath(const char* s) {
    struct charbuf b;
    cb_init(&b, 256);
    replpathsep(&b, s, false);
    return cb_finalize(&b);
}
void sanfilename_cb(const char* s, char r, struct charbuf* cb) {
    char c;
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    uintptr_t b = cb->len;
    #endif
    while ((c = *s)) {
        if (ispathsep(c)) {if (r) cb_add(cb, r);}
        #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        else if ((c >= 1 && c <= 31) || c == '<' || c == '>' || c == ':' ||
            c == '"' || c == '|' || c == '?' || c == '*') {if (r) cb_add(cb, r);}
        #endif
        else cb_add(cb, c);
        ++s;
    }
    // TODO: confirm that checking for illegal names can be omitted on PLAT_NXDK
    #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    if (b == cb->len) return;
    uintptr_t i = b;
    while (i < cb->len) {
        if (cb->data[i] == ' ') cb->data[i] = '_';
        else break;
        ++i;
    }
    if (i == b) {
        char* d = cb->data + b;
        uintptr_t l = cb->len - b;
        if (((l == 3 || (l > 3 && d[3] == '.')) &&
             (!strncasecmp(d, "con", 3) || !strncasecmp(d, "prn", 3) || !strncasecmp(d, "aux", 3) || !strncasecmp(d, "nul", 3))) ||
            ((l == 4 || (l > 4 && d[4] == '.')) &&
             (!strncasecmp(d, "com", 3) || !strncasecmp(d, "lpt", 3)) &&
             ((c >= '0' && c <= '9') || c == '\xB2' || c == '\xB3' || c == '\xB9')) ||
            ((l == 5 || (l > 5 && d[5] == '.')) &&
             (!strncasecmp(d, "com", 3) || !strncasecmp(d, "lpt", 3)) && d[4] == '\xC2' &&
             (c == '\xB2' || c == '\xB3' || c == '\xB9')) ||
            ((l == 6 || (l > 6 && d[6] == '.')) &&
             !strncasecmp(d, "conin$", 6)) ||
            ((l == 7 || (l > 7 && d[7] == '.')) &&
             (!strncasecmp(d, "conout$", 7) || !strncasecmp(d, "conerr$", 7)))) d[2] = (r) ? r : '_';
        if ((l == 1 && d[0] == '.') ||
            (l == 2 && d[0] == '.' && d[1] == '.')) return;
        if (l > 2 && d[0] == '.' && d[1] == '.' && d[2] == '.') d[2] = (r && r != '.') ? r : '_';
    }
    i = cb->len - 1;
    while (i >= b) {
        if (cb->data[i] == '.') --cb->len;
        else break;
        --i;
    }
    while (i >= b) {
        if (cb->data[i] == ' ') cb->data[i] = '_';
        else break;
        --i;
    }
    #endif
}
char* sanfilename(const char* s, char r) {
    struct charbuf cb;
    cb_init(&cb, 64);
    sanfilename_cb(s, r, &cb);
    return cb_finalize(&cb);
}

void restrictpath_cb(const char* s, const char* inseps, char outsep, char outrepl, struct charbuf* cb) {
    long unsigned b = cb->len;
    int ct;
    char** dl = splitstr(s, inseps, false, &ct);
    for (int i = 0; i < ct; ++i) {
        char* d = dl[i];
        if (!*d) goto skip;
        if (d[0] == '.') {
            if (!d[1]) {
                goto skip;
            } else if (d[1] == '.' && !d[2]) {
                while (cb->len > b) {
                    --cb->len;
                    if (cb->data[cb->len] == outsep) break;
                }
                goto skip;
            }
        }
        cb_add(cb, outsep);
        #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        (void)outrepl;
        cb_addstr(cb, d);
        #else
        sanfilename_cb(d, outrepl, cb);
        #endif
        skip:;
    }
    free(*dl);
    free(dl);
}
char* restrictpath(const char* s, const char* inseps, char outsep, char outrepl) {
    struct charbuf cb;
    cb_init(&cb, 256);
    restrictpath_cb(s, inseps, outsep, outrepl, &cb);
    return cb_finalize(&cb);
}

bool md(const char* p) {
    struct charbuf cb;
    cb_init(&cb, 256);
    char c = *p;
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    if (ispathsep(c)) {
        cb_add(&cb, PATHSEP);
        do {
            ++p;
            c = *p;
        } while (ispathsep(c));
    }
    #else
    bool sep = false;
    bool cond = false;
    #endif
    if (!c) return true;
    while (1) {
        if (ispathsep(c) || !c) {
            #if (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            if (!sep) {
                sep = true;
                if (cb.len >= 2 && cb.data[cb.len - 1] == ':') {
                    cond = true;
                    cb_add(&cb, PATHSEP);
                }
            }
            #endif
            cb_nullterm(&cb);
            int t = isFile(cb.data);
            if (t < 0) {
                bool cond;
                #if PLATFORM != PLAT_NXDK
                cond = (mkdir(cb.data) < 0 && errno != EEXIST);
                #else
                cond = (!CreateDirectory(cb.data, NULL) && GetLastError() != ERROR_ALREADY_EXISTS);
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
            if (!c) break;
            do {
                ++p;
                c = *p;
            } while (ispathsep(c));
            #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            cb_add(&cb, PATHSEP);
            #else
            if (cond) {
                cond = false;
            } else {
                cb_add(&cb, PATHSEP);
            }
            #endif
        } else {
            cb_add(&cb, c);
            ++p;
            c = *p;
        }
    }
    cb_dump(&cb);
    return true;
}

char** ls(const char* p, bool ln, int* l) {
    if (!*p) p = "." PATHSEPSTR;
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        DIR* d = opendir(p);
        if (!d) return NULL;
    #else
        WIN32_FIND_DATA fd;
        HANDLE f;
        {
            size_t len = strlen(p);
            char* tmp = malloc(len + 3);
            if (len) memcpy(tmp, p, len);
            tmp[len++] = PATHSEP;
            tmp[len++] = '*';
            tmp[len] = 0;
            f = FindFirstFile(tmp, &fd);
            free(tmp);
            if (f == INVALID_HANDLE_VALUE) return NULL;
        }
    #endif
    char** data = malloc(16 * sizeof(*data));
    int len = 1;
    int size = 16;
    struct charbuf names;
    cb_init(&names, 256);
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        struct dirent* de;
        while ((de = readdir(d))) {
            char* n = de->d_name;
            if (n[0] == '.' && (!n[1] || (n[1] == '.' && !n[2]))) continue; // skip . and ..
            cb_addfake(&names);
            int ol = names.len;
            cb_addstr(&names, p);
            if (!ispathsep(names.data[names.len - 1])) cb_add(&names, PATHSEP);
            cb_addstr(&names, n);
            cb_add(&names, 0);
            struct stat s;
            if (stat(names.data + ol, &s)) {
                names.len = ol - 1;
            } else {
                char i = (S_ISDIR(s.st_mode)) ? LS_ISDIR : 0;
                if (S_ISCHR(s.st_mode)) i |= LS_ISSPL;
                else if (S_ISBLK(s.st_mode)) i |= LS_ISSPL;
                else if (S_ISFIFO(s.st_mode)) i |= LS_ISSPL;
                #ifdef S_ISSOCK
                else if (S_ISSOCK(s.st_mode)) i |= LS_ISSPL;
                #endif
                #if defined(S_ISLNK) && PLATFORM != PLAT_DREAMCAST
                lstat(names.data + ol, &s);
                if (S_ISLNK(s.st_mode)) i |= LS_ISLNK;
                #endif
                names.data[ol - 1] = i;
                if (!ln) {
                    names.len = ol;
                    cb_addstr(&names, n);
                    cb_add(&names, 0);
                }
                if (len == size) {
                    size *= 2;
                    data = realloc(data, size * sizeof(*data));
                }
                data[len++] = (char*)(uintptr_t)ol;
            }
        }
        closedir(d);
    #else
        do {
            if (fd.cFileName[0] == '.' && (!fd.cFileName[1] || (fd.cFileName[1] == '.' && !fd.cFileName[2]))) continue;
            char i = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? LS_ISDIR : 0;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) i |= LS_ISSPL;
            #ifdef FILE_ATTRIBUTE_REPARSE_POINT
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                if (fd.dwReserved0 == IO_REPARSE_TAG_SYMLINK) i |= LS_ISLNK;
                else if (fd.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT) i |= LS_ISLNK;
                #ifdef IO_REPARSE_TAG_LX_SYMLINK
                else if (fd.dwReserved0 == IO_REPARSE_TAG_LX_SYMLINK) i |= LS_ISLNK;
                #endif
                #ifdef IO_REPARSE_TAG_AF_UNIX
                else if (fd.dwReserved0 == IO_REPARSE_TAG_AF_UNIX) i |= LS_ISSPL;
                #endif
                #ifdef IO_REPARSE_TAG_LX_FIFO
                else if (fd.dwReserved0 == IO_REPARSE_TAG_LX_FIFO) i |= LS_ISSPL;
                #endif
                #ifdef IO_REPARSE_TAG_LX_CHR
                else if (fd.dwReserved0 == IO_REPARSE_TAG_LX_CHR) i |= LS_ISSPL;
                #endif
                #ifdef IO_REPARSE_TAG_LX_BLK
                else if (fd.dwReserved0 == IO_REPARSE_TAG_LX_BLK) i |= LS_ISSPL;
                #endif
                else i |= LS_ISLNK;
            }
            #endif
            cb_add(&names, i);
            if (len == size) {
                size *= 2;
                data = realloc(data, size * sizeof(*data));
            }
            data[len++] = (char*)(uintptr_t)names.len;
            if (ln) {
                cb_addstr(&names, p);
                if (!ispathsep(names.data[names.len - 1])) cb_add(&names, PATHSEP);
            }
            cb_addstr(&names, fd.cFileName);
            cb_add(&names, 0);
        } while (FindNextFile(f, &fd));
        FindClose(f);
    #endif
    names.data = realloc(names.data, names.len);
    data = realloc(data, (len + 1) * sizeof(*data));
    *data = names.data;
    data[len] = NULL;
    --len;
    ++data;
    for (int i = 0; i < len; ++i) {
        data[i] += (uintptr_t)names.data;
    }
    if (l) *l = len;
    return data;
}
void freels(char** d) {
    --d;
    free(*d);
    free(d);
}

bool startls(const char* p, struct lsstate* s) {
    if (!*p) p = "." PATHSEPSTR;
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        s->d = opendir(p);
        if (!s->d) return false;
    #else
        size_t len = strlen(p);
        char* tmp = malloc(len + 3);
        if (len) memcpy(tmp, p, len);
        tmp[len++] = PATHSEP;
        tmp[len++] = '*';
        tmp[len] = 0;
        s->f = FindFirstFile(tmp, &s->fd);
        free(tmp);
        if (s->f == INVALID_HANDLE_VALUE) return false;
        s->r = false;
    #endif
    cb_init(&s->p, 256);
    cb_addstr(&s->p, p);
    if (!ispathsep(s->p.data[s->p.len - 1])) cb_add(&s->p, PATHSEP);
    return true;
}
bool getls(struct lsstate* s, const char** on, const char** oln) {
    tryagain:;
    char* n;
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        struct dirent* de = readdir(s->d);
        if (!de) return false;
        n = de->d_name;
    #else
        if (s->r) {
            if (!FindNextFile(s->f, &s->fd)) return false;
        } else {
            s->r = true;
        }
        n = s->fd.cFileName;
    #endif
    if (n[0] == '.' && (!n[1] || (n[1] == '.' && !n[2]))) goto tryagain;
    if (on) *on = n;
    if (oln) {
        int l = s->p.len;
        cb_addstr(&s->p, n);
        cb_nullterm(&s->p);
        s->p.len = l;
        *oln = s->p.data;
    }
    return true;
}
void endls(struct lsstate* s) {
    cb_dump(&s->p);
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        closedir(s->d);
    #else
        FindClose(s->f);
    #endif
}

#if 0
bool rm(const char* p) {
    
}
#endif
