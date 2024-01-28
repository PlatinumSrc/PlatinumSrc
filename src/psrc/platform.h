#ifndef PSRC_PLATFORM_H
#define PSRC_PLATFORM_H

#define PLAT_UNKNOWN 0
#define PLAT_ANDROID 1
#define PLAT_EMSCR   2
#define PLAT_FREEBSD 3
#define PLAT_HAIKU   4
#define PLAT_LINUX   5
#define PLAT_MACOS   6
#define PLAT_NETBSD  7
#define PLAT_OPENBSD 8
#define PLAT_UNIX    9
#define PLAT_WIN32   10
#define PLAT_NXDK    11
#define PLAT__COUNT  12

#define ARCH_UNKNOWN 0
#define ARCH_AMD64   1
#define ARCH_ARM     2
#define ARCH_ARM64   3
#define ARCH_I386    4
#define ARCH_WASM    5

#define BO_LE 1234
#define BO_BE 4321

#if defined(__ANDROID__)
    #define PLATFORM PLAT_ANDROID
#elif defined(__EMSCRIPTEN__)
    #define PLATFORM PLAT_EMSCR
#elif defined(__FreeBSD__)
    #define PLATFORM PLAT_FREEBSD
#elif defined(__HAIKU__)
    #define PLATFORM PLAT_HAIKU
#elif defined(__linux__)
    #define PLATFORM PLAT_LINUX
#elif defined(macintosh) || defined(Macintosh) || (defined(__APPLE__) && defined(__MACH__))
    #define PLATFORM PLAT_MACOS
#elif defined(__NetBSD__)
    #define PLATFORM PLAT_NETBSD
#elif defined(__OpenBSD__)
    #define PLATFORM PLAT_OPENBSD
#elif defined(__unix__)
    #define PLATFORM PLAT_UNIX
#elif defined(_WIN32) && !(defined(NXDK) || defined(_XBOX))
    #define PLATFORM PLAT_WIN32
#elif defined(NXDK)
    #define PLATFORM PLAT_NXDK
#else
    #define PLATFORM PLAT_UNKNOWN
    #warning Unknown or unsupported platform. \
    This will probably result in a broken build.
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
    #define ARCH ARCH_AMD64
    #define ARCHSTR "x86 64-bit"
    #ifndef BYTEORDER
        #define BYTEORDER BO_LE
    #endif
#elif defined(__arm__) && !defined(__aarch64__)
    #define ARCH ARCH_ARM
    #define ARCHSTR "Arm"
    #ifndef BYTEORDER
        #if defined(__ARMEL__)
            #define BYTEORDER BO_LE
        #elif defined(__ARMEB__)
            #define BYTEORDER BO_BE
        #endif
    #endif
#elif defined(__aarch64__)
    #define ARCH ARCH_ARM64
    #define ARCHSTR "Arm 64-bit"
    #ifndef BYTEORDER
        #if defined(__AARCH64EL__)
            #define BYTEORDER BO_LE
        #elif defined(__AARCH64EB__)
            #define BYTEORDER BO_BE
        #endif
    #endif
#elif defined(__i386__) || defined(__i386)
    #define ARCH ARCH_I386
    #define ARCHSTR "x86"
    #ifndef BYTEORDER
        #define BYTEORDER BO_LE
    #endif
#elif defined(__wasm) || defined(__wasm__) || defined(__WASM__)
    #define ARCH ARCH_WASM
    #define ARCHSTR "WebAssembly"
#else
    #define ARCH ARCH_UNKNOWN
    #define ARCHSTR "Unknown"
    #warning Unknown or unsupported architecture. \
    This may result in a broken build.
#endif

#ifndef BYTEORDER
    #if defined(__LITTLE_ENDIAN__)
        #define BYTEORDER BO_LE
    #elif defined(__BIG_ENDIAN__)
        #define BYTEORDER BO_BE
    #else
        #ifdef __BYTE_ORDER__
            #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
                #define BYTEORDER BO_LE
            #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                #define BYTEORDER BO_BE
            #endif
        #endif
        #ifndef BYTEORDER
            #ifdef __BYTE_ORDER
                #if __BYTE_ORDER == __LITTLE_ENDIAN
                    #define BYTEORDER BO_LE
                #elif __BYTE_ORDER == __BIG_ENDIAN
                    #define BYTEORDER BO_BE
                #endif
            #endif
        #endif
    #endif
    #ifndef BYTEORDER
        #warning Unknown or unsupported byte order. \
        This will probably result in a broken build.
    #endif
#endif

extern const char* platname[PLAT__COUNT];
extern const char* platdir[PLAT__COUNT];
extern const char* altplatdir[PLAT__COUNT];

#endif
