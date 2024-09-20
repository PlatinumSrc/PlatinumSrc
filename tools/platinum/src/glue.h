#ifndef PSRC_GLUE_H
#define PSRC_GLUE_H

#ifndef _WIN32
    #define mkdir(x) mkdir(x, (S_IRWXU) | (S_IRGRP | S_IXGRP) | (S_IROTH | S_IXOTH))
#else
    #define pause() Sleep(INFINITE)
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    #define strdup _strdup
    #if PLATFORM != PLAT_NXDK
        #define realpath(x, y) _fullpath(y, x, 0)
    #else
        #define realpath(x, y) (NULL)
    #endif
    #define mkdir(x) CreateDirectory(x, NULL)
    #define fileno _fileno
    char* basename(char*);
#endif

#ifndef _WIN32
    #define PRIdz "zd"
    #define PRIuz "zu"
    #define PRIxz "zx"
    #define PRIXz "zX"
#else
    #define PRIdz "Id"
    #define PRIuz "Iu"
    #define PRIxz "Ix"
    #define PRIXz "IX"
#endif

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#endif
