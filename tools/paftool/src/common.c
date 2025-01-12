#include "common.h"

#include <psrc/crc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifndef _WIN32
    #include <sys/stat.h>
#else
    #include <windows.h>
#endif

int isFile(const char* p) {
    #ifndef _WIN32
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

void ftree_init(struct ftree* tree, const char* argv0, int ct, const char** pl) {
    VLB_INIT(*tree, 16, VLB_OOM_NOP);
    for (int i = 0; i < ct; ++i) {
        const char* p = pl[i];
        int t = isFile(p);
        if (t < 0) {
            fprintf(stderr, "%s: Could not stat '%s': %s\n", argv0, p, strerror(errno));
            continue;
        }
        if (t > 1) {
            fprintf(stderr, "%s: '%s' is a special file\n", argv0, p);
            continue;
        }
        while (ispathsep(*p)) ++p;
        struct ftreenode* n;
        VLB_NEXTPTR(*tree, n, 3, 2, VLB_OOM_NOP);
    }
    VLB_FREE(*tree);
}
