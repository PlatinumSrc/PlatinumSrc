#ifndef PSRC_FILESYSTEM_H
#define PSRC_FILESYSTEM_H

#include "string.h"

#include "platform.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #include <dirent.h>
#else
    #include <windows.h>
#endif

#include "attribs.h"

#if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #define PATHSEP '/'
    #define PATHSEPSTR "/"
#else
    #define PATHSEP '\\'
    #define PATHSEPSTR "\\"
#endif

#define LS_ISDIR (1 << 0)
#define LS_ISLNK (1 << 1)
#define LS_ISSPL (1 << 2)

#if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #define ispathsep(c) ((c) == '/')
#else
    static ALWAYSINLINE bool ispathsep(char c) {
        return (c == '/' || c == '\\');
    }
#endif

struct lsstate {
    struct charbuf p;
    #if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    DIR* d;
    #else
    WIN32_FIND_DATA fd;
    HANDLE f;
    bool r;
    #endif
};

int isFile(const char*);
long getFileSize(FILE* file, bool close);
char* basepathname(char*);
void replpathsep(struct charbuf* cb, const char*, bool first);
char* mkpath(const char*, ...);
char* strpath(const char*);
char* strrelpath(const char*);
void sanfilename_cb(const char*, char repl, struct charbuf*);
char* sanfilename(const char*, char repl);
void restrictpath_cb(const char*, const char* inseps, char outsep, char outrepl, struct charbuf*);
char* restrictpath(const char*, const char* inseps, char outsep, char outrepl);
bool md(const char*);
char** ls(const char*, bool longnames, size_t* l); // info flags are stored at [-1] of each string
bool startls(const char*, struct lsstate*);
bool getls(struct lsstate*, const char** name, const char** longname);
void endls(struct lsstate*);
void freels(char**);
bool rm(const char*);

#endif
