#ifndef PSRC_PLATFORM_H
#define PSRC_PLATFORM_H

#define PLAT_UNKNOWN 0
#define PLAT_ANDROID 1
#define PLAT_FREEBSD 2
#define PLAT_LINUX   3
#define PLAT_MACOS   4
#define PLAT_NETBSD  5
#define PLAT_OPENBSD 6
#define PLAT_UNIX    7
#define PLAT_WINDOWS 8
#define PLAT_NXDK    9

#define ARCH_UNKNOWN 0
#define ARCH_AMD64   1
#define ARCH_ARM     2
#define ARCH_ARM64   3
#define ARCH_I386    4

#define BO_LE 1234
#define BO_BE 4321

#if defined(__ANDROID__)
    #define PLATFORM PLAT_ANDROID
    #define PLATSTR "Android"
    #define PLATDIR "android"
#elif defined(__FreeBSD__)
    #define PLATFORM PLAT_FREEBSD
    #define PLATSTR "FreeBSD"
    #define PLATDIR "freebsd"
#elif defined(__linux__)
    #define PLATFORM PLAT_LINUX
    #define PLATSTR "Linux"
    #define PLATDIR "linux"
#elif defined(macintosh) || defined(Macintosh) || (defined(__APPLE__) && defined(__MACH__))
    #define PLATFORM PLAT_MACOS
    #define PLATSTR "MacOS"
    #define PLATDIR "macos"
#elif defined(__NetBSD__)
    #define PLATFORM PLAT_NETBSD
    #define PLATSTR "NetBSD"
    #define PLATDIR "netbsd"
#elif defined(__OpenBSD__)
    #define PLATFORM PLAT_OPENBSD
    #define PLATSTR "OpenBSD"
    #define PLATDIR "openbsd"
#elif defined(__unix__)
    #define PLATFORM PLAT_UNIX
    #define PLATSTR "Unix"
    #define PLATDIR "unix"
#elif defined(_WIN32) && !(defined(NXDK) || defined(_XBOX))
    #define PLATFORM PLAT_WINDOWS
    #define PLATSTR "Windows"
    #define PLATDIR "windows"
#elif defined(NXDK)
    #define PLATFORM PLAT_NXDK
    #define PLATSTR "Xbox (NXDK)"
    #define PLATDIR "nxdk"
#else
    #define PLATFORM PLAT_UNKNOWN
    #define PLATSTR "Unknown"
    #warning Unknown platform. \
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
#else
    #define ARCH ARCH_UNKNOWN
    #define ARCHSTR "Unknown"
    #warning Unknown OS. \
    This will probably result in a broken build.
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
        #warning Unknown byte order. \
        This will probably result in a broken build.
    #endif
#endif

#endif
