#include "../rcmgralloc.h"

#include "pkd.h"

#include <string.h>
#if (PLATFLAGS & PLATFLAG_UNIXLIKE)
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
#elif PLATFORM == PLAT_WIN32
    #include <windows.h>
#endif

int pkd_open(const char* p, bool nolock, struct pkd* d) {
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE) || PLATFORM == PLAT_WIN32
        char* lockpath;
        if (!nolock) {
            size_t lockpathlen = strlen(p);
            lockpath = malloc(lockpathlen + 6);
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
            #elif PLATFORM == PLAT_WIN32
                HANDLE* h = CreateFile(lockpath, 0, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
                if (!h) {
                    free(lockpath);
                    return PKD_ERR_OPEN_LOCKED;
                }
                CloseHandle(h);
            #endif
        } else {
            lockpath = NULL;
        }
    #endif
    FILE* f = fopen(p, "w+b");
    if (!f) {
        #if (PLATFLAGS & PLATFLAG_UNIXLIKE)
            if (!nolock) {
                unlink(lockpath);
                free(lockpath);
            }
        #elif PLATFORM == PLAT_WIN32
            if (!nolock) {
                DeleteFile(lockpath);
                free(lockpath);
            }
        #endif
        return PKD_ERR_OPEN_CANTOPEN;
    }
    d->f = f;
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE) || PLATFORM == PLAT_WIN32
        d->lockpath = lockpath;
    #endif
    return PKD_ERR_NONE;
}

void pkd_close(struct pkd* d) {
    fclose(d->f);
    #if (PLATFLAGS & PLATFLAG_UNIXLIKE)
        if (d->lockpath) {
            unlink(d->lockpath);
            free(d->lockpath);
        }
    #elif PLATFORM == PLAT_WIN32
        if (d->lockpath) {
            DeleteFile(d->lockpath);
            free(d->lockpath);
        }
    #endif
}
