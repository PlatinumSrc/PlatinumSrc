#ifndef PSRC_COMMON_BYTEORDER
#define PSRC_COMMON_BYTEORDER

#include "../platform.h"
#include "../attribs.h"

#include <stdint.h>

#define swapbo16(v) ((uint16_t)((((v) << 8) & (uint16_t)0xFF00) | (((v) >> 8) & (uint16_t)0xFF)))
#define swapbo32(v) ((uint32_t)((((v) << 24) & (uint32_t)0xFF000000) | (((v) << 8) & (uint32_t)0xFF0000) |\
                    (((v) >> 8) & (uint32_t)0xFF00) | (((v) >> 24) & (uint32_t)0xFF)))
#define swapbo64(v) ((uint64_t)((((v) << 56) & (uint64_t)0xFF00000000000000) | (((v) << 40) & (uint64_t)0xFF000000000000) |\
                    (((v) << 24) & (uint64_t)0xFF0000000000) | (((v) << 8) & (uint64_t)0xFF00000000) |\
                    (((v) >> 8) & (uint64_t)0xFF000000) | (((v) >> 24) & (uint64_t)0xFF0000) |\
                    (((v) >> 40) & (uint64_t)0xFF00) | (((v) >> 56) & (uint64_t)0xFF)))

#if BYTEORDER == BO_BE
    #define swapbe16(v) ((uint16_t)(v))
    #define swapbe32(v) ((uint32_t)(v))
    #define swapbe64(v) ((uint64_t)(v))
    #define swapbefloat(v) ((float)(v))
    static ALWAYSINLINE uint16_t swaple16(uint16_t v) {return swapbo16(v);}
    static ALWAYSINLINE uint32_t swaple32(uint32_t v) {return swapbo32(v);}
    static ALWAYSINLINE uint64_t swaple64(uint64_t v) {return swapbo64(v);}
    static ALWAYSINLINE float swaplefloat(float v) {
        char* d = (char*)&v;
        char tmp = d[0];
        d[0] = d[3];
        d[3] = tmp;
        tmp = d[1];
        d[1] = d[2];
        d[2] = tmp;
        return v;
    }
#else
    static ALWAYSINLINE uint16_t swapbe16(uint16_t v) {return swapbo16(v);}
    static ALWAYSINLINE uint32_t swapbe32(uint32_t v) {return swapbo32(v);}
    static ALWAYSINLINE uint64_t swapbe64(uint64_t v) {return swapbo64(v);}
    static ALWAYSINLINE float swapbefloat(float v) {
        char* d = (char*)&v;
        char tmp = d[0];
        d[0] = d[3];
        d[3] = tmp;
        tmp = d[1];
        d[1] = d[2];
        d[2] = tmp;
        return v;
    }
    #define swaple16(v) ((uint16_t)(v))
    #define swaple32(v) ((uint32_t)(v))
    #define swaple64(v) ((uint64_t)(v))
    #define swaplefloat(v) ((float)(v))
#endif

#endif
