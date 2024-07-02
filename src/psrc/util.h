#ifndef PSRC_UTIL_H
#define PSRC_UTIL_H

#ifndef _MSC_VER
    #define PACKEDSTRUCT(...) struct __attribute__((__packed__)) __VA_ARGS__
    #define PACKEDUNION(...) union __attribute__((__packed__)) __VA_ARGS__
    #define PACKEDENUM(...) enum __attribute__((__packed__)) __VA_ARGS__
    #define FORCEINLINE inline __attribute__((always_inline))
    #define THREADLOCAL __thread
#else
    #define PACKEDSTRUCT(...) __pragma(pack(push, 1)) struct __VA_ARGS__ __pragma(pack(pop))
    #define PACKEDUNION(...) __pragma(pack(push, 1)) union __VA_ARGS__ __pragma(pack(pop))
    #define PACKEDENUM(...) enum __VA_ARGS__
    #define FORCEINLINE inline __forceinline
    #define THREADLOCAL __declspec(thread)
#endif

#endif
