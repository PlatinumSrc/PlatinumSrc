#ifndef PSRC_COMMON_BYTEORDER
#define PSRC_COMMON_BYTEORDER

#include "../platform.h"

#include <stdint.h>

#define swapbo16(x) ((uint16_t)((((x) << 8) & 0xFF00U) | (((x) >> 8) & 0xFFU)))
#define swapbo32(x) ((uint32_t)((((x) << 24) & 0xFF000000LU) | (((x) << 8) & 0xFF0000LU) |\
                    (((x) >> 8) & 0xFF00LU) | (((x) >> 24) & 0xFFLU)))
#define swapbo64(x) ((uint64_t)((((x) << 56) & 0xFF00000000000000LLU) |(((x) << 40) & 0xFF000000000000LLU) |\
                    (((x) << 24) & 0xFF0000000000LLU) | (((x) << 8) & 0xFF00000000LLU) |\
                    (((x) >> 8) & 0xFF000000LLU) | (((x) >> 24) & 0xFF0000LLU) |\
                    (((x) >> 40) & 0xFF00LLU) | (((x) >> 56) & 0xFFLLU)))

#if BYTEORDER == BO_BE
    #define swapbe16(x) ((uint16_t)(x))
    #define swapbe32(x) ((uint32_t)(x))
    #define swapbe64(x) ((uint64_t)(x))
    #define swaple16(x) (swapbo16(((uint16_t)(x))))
    #define swaple32(x) (swapbo32(((uint32_t)(x))))
    #define swaple64(x) (swapbo64(((uint64_t)(x))))
    static inline void swapinplacebe16(uint16_t* p) {(void)p;}
    static inline void swapinplacebe32(uint32_t* p) {(void)p;}
    static inline void swapinplacebe64(uint64_t* p) {(void)p;}
    static inline void swapinplacele16(uint16_t* p) {uint16_t v = *p; *p = swapbo16(v);}
    static inline void swapinplacele32(uint32_t* p) {uint32_t v = *p; *p = swapbo32(v);}
    static inline void swapinplacele64(uint64_t* p) {uint64_t v = *p; *p = swapbo64(v);}
#else
    #define swapbe16(x) (swapbo16(((uint16_t)(x))))
    #define swapbe32(x) (swapbo32(((uint32_t)(x))))
    #define swapbe64(x) (swapbo64(((uint64_t)(x))))
    #define swaple16(x) ((uint16_t)(x))
    #define swaple32(x) ((uint32_t)(x))
    #define swaple64(x) ((uint64_t)(x))
    static inline void swapinplacebe16(uint16_t* p) {uint16_t v = *p; *p = swapbo16(v);}
    static inline void swapinplacebe32(uint32_t* p) {uint32_t v = *p; *p = swapbo32(v);}
    static inline void swapinplacebe64(uint64_t* p) {uint64_t v = *p; *p = swapbo64(v);}
    static inline void swapinplacele16(uint16_t* p) {(void)p;}
    static inline void swapinplacele32(uint32_t* p) {(void)p;}
    static inline void swapinplacele64(uint64_t* p) {(void)p;}
#endif

#endif
