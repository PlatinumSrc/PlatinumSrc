#ifndef AUX_FS_H
#define AUX_FS_H

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

int isFile(const char* path);
long getFileSize(FILE* file, bool close);
char* mkpath(const char*, ...);

#endif
