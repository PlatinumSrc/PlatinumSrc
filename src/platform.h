#ifndef PLATFORM_H
#define PLATFORM_H

#define PLAT_UNKNOWN 0
#define PLAT_ANDROID 1
#define PLAT_FREEBSD 2
#define PLAT_LINUX   3
#define PLAT_MACOS   4
#define PLAT_NETBSD  5
#define PLAT_OPENBSD 6
#define PLAT_UNIX    7
#define PLAT_WINDOWS 8
#define PLAT_XBOX    9

#define ARCH_UNKNOWN 0
#define ARCH_AMD64   1
#define ARCH_ARM     2
#define ARCH_ARM64   3
#define ARCH_I386    4

#if defined(__ANDROID__)
    #define PLATFORM PLAT_ANDROID
    #define PLATSTR "Android"
#elif defined(__FreeBSD__)
    #define PLATFORM PLAT_FREEBSD
    #define PLATSTR "FreeBSD"
#elif defined(__linux__)
    #define PLATFORM PLAT_LINUX
    #define PLATSTR "Linux"
#elif defined(macintosh) || defined(Macintosh) || (defined(__APPLE__) && defined(__MACH__))
    #define PLATFORM PLAT_MACOS
    #define PLATSTR "MacOS"
#elif defined(__NetBSD__)
    #define PLATFORM PLAT_NETBSD
    #define PLATSTR "NetBSD"
#elif defined(__OpenBSD__)
    #define PLATFORM PLAT_OPENBSD
    #define PLATSTR "OpenBSD"
#elif defined(__unix__)
    #define PLATFORM PLAT_UNIX
    #define PLATSTR "Unix or Unix-like"
#elif defined(_WIN32) && !(defined(NXDK) || defined(_XBOX))
    #define PLATFORM PLAT_WINDOWS
    #define PLATSTR "Windows"
#elif defined(NXDK) || defined(_XBOX)
    #define PLATFORM PLAT_XBOX
    #define PLATSTR "Xbox"
#else
    #define PLATFORM PLAT_UNKNOWN
    #define PLATSTR "Unknown"
    #warning Unknown platform. \
    This will probably result in a broken build.
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
    #define ARCH ARCH_AMD64
    #define ARCHSTR "x86 64-bit"
#elif defined(__arm__) && !defined(__aarch64__)
    #define ARCH ARCH_ARM
    #define ARCHSTR "Arm"
#elif defined(__aarch64__)
    #define ARCH ARCH_ARM64
    #define ARCHSTR "Arm 64-bit"
#elif defined(__i386__) || defined(__i386)
    #define ARCH ARCH_I386
    #define ARCHSTR "x86"
#else
    #define ARCH ARCH_UNKNOWN
    #define ARCHSTR "Unknown"
    #warning Unknown OS. \
    This will probably result in a broken build.
#endif

#endif
