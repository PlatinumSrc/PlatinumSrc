#ifndef PSRC_AUX_FILESYSTEM_H
#define PSRC_AUX_FILESYSTEM_H

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

int isFile(const char*);
long getFileSize(FILE* file, bool close);
char* mkpath(const char*, ...);
char* strpath(const char*);
char* strrelpath(const char*);
bool md(const char*);
bool rm(const char*);

#endif
