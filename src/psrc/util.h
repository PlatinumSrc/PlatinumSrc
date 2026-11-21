#ifndef PSRC_UTIL_H
#define PSRC_UTIL_H

#define STR__EVAL(STR__EVAL__x) #STR__EVAL__x
#define STR(STR__x) STR__EVAL(STR__x)

#define TOPTR(TOPTR__x) ((void*)(uintptr_t)(TOPTR__x))
#define FROMPTR(FROMPTR__x) ((uintptr_t)(void*)(FROMPTR__x))

#ifndef PSRC_DBGLVL
    #define DEFAULTCASE(DEFAULTCASE__x) case DEFAULTCASE__x
#else
    #define DEFAULTCASE(DEFAULTCASE__x) default
#endif

#endif
