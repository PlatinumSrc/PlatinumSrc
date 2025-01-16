#include "../rcmgralloc.h"

#include "pkd.h"

#include <string.h>
#if !(PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
#elif (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
    #include <windows.h>
#endif

int pkd_open(const char* p, struct pkd* d) {
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE) || (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
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
            HANDLE* h = CreateFile(lockpath, 0, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
            if (!h) {
                free(lockpath);
                return PKD_ERR_OPEN_LOCKED;
            }
            CloseHandle(h);
        #endif
    #endif
    FILE* f = fopen(p, "w+b");
    if (!f) {
        #if (PLATFLAGS & PLATFLAG_UNIXLIKE)
            unlink(lockpath);
        #elif (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            DeleteFile(lockpath);
        #endif
        #if (PLATFLAGS & PLATFLAG_UNIXLIKE) || (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
            free(lockpath);
        #endif
        return PKD_ERR_OPEN_CANTOPEN;
    }
    d->f = f;
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE) || (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        d->lockpath = lockpath;
    #endif
    return PKD_ERR_NONE;
}

void pkd_close(struct pkd* d) {
    fclose(d->f);
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE)
        unlink(d->lockpath);
    #elif (PLATFLAGS & PLATFLAG_WINDOWSLIKE)
        DeleteFile(d->lockpath);
    #endif
}
