#include "fs.h"

#include <stddef.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdbool.h>

int isFile(char* p) {
    struct stat s;
    if (stat(p, &s)) return -1;
    if (S_ISREG(s.st_mode)) return 1;
    if (S_ISDIR(s.st_mode)) return 0;
    return 2;
}

long getFileSize(FILE* f, bool c) {
    if (!f) return -1;
    long ret = 0;
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
