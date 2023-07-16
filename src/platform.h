#ifndef PLATFORM_H
#define PLATFORM_H

#define OS_UNKNOWN 0
#define OS_ANDROID 1
#define OS_FREEBSD 2
#define OS_LINUX   3
#define OS_MACOS   4
#define OS_NETBSD  5
#define OS_OPENBSD 6
#define OS_UNIX    7
#define OS_WINDOWS 8

#define ARCH_UNKNOWN 0
#define ARCH_AMD64   1
#define ARCH_ARM     2
#define ARCH_ARM64   3
#define ARCH_I386    4

#if defined(__ANDROID__)
    #define OS OS_ANDROID
    #define OSSTR "Android"
#elif defined(__FreeBSD__)
    #define OS OS_FREEBSD
    #define OSSTR "FreeBSD"
#elif defined(__linux__)
    #define OS OS_LINUX
    #define OSSTR "Linux"
#elif defined(macintosh) || defined(Macintosh) || (defined(__APPLE__) && defined(__MACH__))
    #define OS OS_MACOS
    #define OSSTR "MacOS"
#elif defined(__NetBSD__)
    #define OS OS_NETBSD
    #define OSSTR "NetBSD"
#elif defined(__OpenBSD__)
    #define OS OS_OPENBSD
    #define OSSTR "OpenBSD"
#elif defined(__unix__)
    #define OS OS_UNIX
    #define OSSTR "Unix or Unix-like"
#elif defined(_WIN32)
    #define OS OS_WINDOWS
    #define OSSTR "Windows"
#else
    #define OS OS_UNKNOWN
    #define OSSTR "Unknown"
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
#endif

#endif
