#include "common.h"

#include <psrc/crc.h>
#include <psrc/string.h>
#include <psrc/filesystem.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#ifndef _WIN32
    #include <sys/stat.h>
#else
    #include <windows.h>
#endif

void ftree_init(struct ftree* tree, int ct, char** pl) {
    VLB_INIT(*tree, 16, VLB_OOM_NOP);
    for (int i = 0; i < ct; ++i) {
        const char* p = pl[i];
        int isfile = isFile(p);
        if (isfile < 0) {
            fprintf(stderr, "Warning: Could not stat '%s': %s\n", p, strerror(errno));
            continue;
        }
        if (isfile > 1) {
            fprintf(stderr, "Warning: Ignoring '%s' as it is a special file\n", p);
            continue;
        }
        int namect;
        char** names;
        {
            char* tmp = strrelpath(p);
            names = splitstr(p, PATHSEPSTR, false, &namect);
            free(tmp);
        }
        struct ftree* curtree = tree;
        for (int j = 0; j < namect - 1; ++j) {
            if (!*names[j]) continue;
            uint32_t namecrc = strcrc32(names[j]);
            for (uintptr_t nodei = 0; nodei < curtree->len; ++nodei) {
                //if (
            }
        }
        if (isfile) {
            
        } else {
            
        }
    }
}
void ftree_free(struct ftree* tree) {
    VLB_FREE(*tree);
}
