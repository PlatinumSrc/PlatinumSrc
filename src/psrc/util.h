#ifndef PSRC_UTIL_H
#define PSRC_UTIL_H

#ifndef _MSC_VER
    #define PACKED __attribute__((__packed__))
    #define PACKEDSTRUCT(...) struct __attribute__((__packed__)) __VA_ARGS__
    #define PACKEDUNION(...) union __attribute__((__packed__)) __VA_ARGS__
    #define PACKEDENUM(...) enum __attribute__((__packed__)) __VA_ARGS__
    #define FORCEINLINE inline __attribute__((always_inline))
    #define THREADLOCAL __thread
#else
    #define PACKED
    #define PACKEDSTRUCT(...) __pragma(pack(push, 1)) struct __VA_ARGS__ __pragma(pack(pop))
    #define PACKEDUNION(...) __pragma(pack(push, 1)) union __VA_ARGS__ __pragma(pack(pop))
    #define PACKEDENUM(...) enum __VA_ARGS__
    #ifndef FORCEINLINE
        #define FORCEINLINE __forceinline
    #endif
    #define THREADLOCAL __declspec(thread)
#endif

#endif
