#ifndef PSRC_COMMON_FILESYSTEM_H
#define PSRC_COMMON_FILESYSTEM_H

#include "../platform.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #define PATHSEP '/'
    #define PATHSEPSTR "/"
#else
    #define PATHSEP '\\'
    #define PATHSEPSTR "\\"
#endif

#define LS_ISDIR (1 << 0)
#define LS_ISLNK (1 << 1)

int isFile(const char*);
long getFileSize(FILE* file, bool close);
char* mkpath(const char*, ...);
char* strpath(const char*);
char* strrelpath(const char*);
bool md(const char*);
char** ls(const char*); // info flags are stored at [-1] of each string
void freels(char**);
bool rm(const char*);

char* mkmaindir(void);
char* mkuserdir(const char* maindir, const char* name);

#endif
