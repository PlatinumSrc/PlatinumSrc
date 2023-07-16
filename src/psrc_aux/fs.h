#ifndef AUX_FS_H
#define AUX_FS_H

#include <stdio.h>
#include <stdbool.h>

int isFile(char* path);
long getFileSize(FILE* file, bool close);

#endif
