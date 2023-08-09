#ifndef AUX_FILESYSTEM_H
#define AUX_FILESYSTEM_H

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

int isFile(const char* path);
long getFileSize(FILE* file, bool close);
char* mkpath(const char*, ...);

#endif
