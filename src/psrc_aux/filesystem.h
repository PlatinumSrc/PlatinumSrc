#ifndef AUX_FILESYSTEM_H
#define AUX_FILESYSTEM_H

#include "../platform.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#if PLATFORM != PLAT_WINDOWS && PLATFORM != PLAT_XBOX
    #define PATHSEP '/'
    #define PATHSEPSTR "/"
#else
    #define PATHSEP '\\'
    #define PATHSEPSTR "\\"
#endif

enum dirindex {
    DIR_MAIN,
    DIR_SELF,
    DIR_USER,
    DIR__COUNT,
};

int isFile(const char*);
long getFileSize(FILE* file, bool close);
char* mkpath(const char*, ...);
bool md(const char*);
bool rm(const char*);

extern char* dirs[DIR__COUNT];

#endif
