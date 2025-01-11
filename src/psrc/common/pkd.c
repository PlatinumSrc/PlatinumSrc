#include "../rcmgralloc.h"

#include "pkd.h"

#include <string.h>
#if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
#else
    #include <windows.h>
#endif

int pkd_open(const char* p, struct pkd* d) {
    int lockpathlen = strlen(p);
    char* lockpath = malloc(lockpathlen + 6);
    memcpy(lockpath, p, lockpathlen);
    lockpath[lockpathlen++] = '.';
    lockpath[lockpathlen++] = 'l';
    lockpath[lockpathlen++] = 'o';
    lockpath[lockpathlen++] = 'c';
    lockpath[lockpathlen++] = 'k';
    lockpath[lockpathlen] = 0;
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE)
        int fd = open(lockpath, O_CREAT | O_EXCL, S_IRUSR | S_IRGRP | S_IROTH);
        if (fd < 0) {
            free(lockpath);
            return PKD_ERR_OPEN_LOCKED;
        }
        close(fd);
    #elif (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        HANDLE* h = CreateFile(lockpath, 0, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL);
        if (!h) {
            free(lockpath);
            return PKD_ERR_OPEN_LOCKED;
        }
        CloseHandle(h);
    #endif
    FILE* f = fopen(p, "w+b");
    if (!f) {
        #if (PLATFLAGS & PLATFLAG_UNIXLIKE)
            unlink(lockpath);
        #else
            DeleteFile(lockpath);
        #endif
        free(lockpath);
        return PKD_ERR_OPEN_CANTOPEN;
    }
    d->f = f;
    d->lockpath = lockpath;
    return PKD_ERR_NONE;
}

void pkd_close(struct pkd* d) {
    fclose(d->f);
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE)
        unlink(d->lockpath);
    #else
        DeleteFile(d->lockpath);
    #endif
}
