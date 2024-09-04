#ifndef PSRC_ATTRIBS_H
#define PSRC_ATTRIBS_H

#ifndef _MSC_VER
    #define PACKEDENUM enum __attribute__((packed))
    #define ALWAYSINLINE inline __attribute__((always_inline))
    #define THREADLOCAL __thread
#else
    #define PACKEDENUM enum
    #define ALWAYSINLINE __forceinline
    #define THREADLOCAL __declspec(thread)
#endif

#endif
