#ifndef PSRC_PLATFORM_H
#define PSRC_PLATFORM_H

#define PLAT_UNKNOWN   0
#define PLAT_3DS       1
#define PLAT_ANDROID   2
#define PLAT_DREAMCAST 3
#define PLAT_EMSCR     4
#define PLAT_FREEBSD   5
#define PLAT_GAMECUBE  6
#define PLAT_GDK       7
#define PLAT_HAIKU     8
#define PLAT_LINUX     9
#define PLAT_MACOS     10
#define PLAT_NETBSD    11
#define PLAT_OPENBSD   12
#define PLAT_PS2       13
#define PLAT_UNIX      14
#define PLAT_WII       15
#define PLAT_WIN32     16
#define PLAT_NXDK      17
#define PLAT__COUNT    18

#define ARCH_UNKNOWN 0
#define ARCH_AMD64   1
#define ARCH_ARM     2
#define ARCH_ARM64   3
#define ARCH_I386    4
#define ARCH_MIPS    5
#define ARCH_PPC     6
#define ARCH_SUPERH  7
#define ARCH_WASM    8
#define ARCH__COUNT  9

#define BO_LE 1234
#define BO_BE 4321

#define PLATFLAG_UNIXLIKE (1 << 0)
#define PLATFLAG_WINDOWSLIKE (1 << 1)

#if defined(__3DS__)
    #define PLATFORM PLAT_3DS
    #define PLATFLAGS (0)
#elif defined(__ANDROID__)
    #define PLATFORM PLAT_ANDROID
    #define PLATFLAGS (PLATFLAG_UNIXLIKE)
#elif defined(_arch_dreamcast)
    #define PLATFORM PLAT_DREAMCAST
    #define PLATFLAGS (0)
#elif defined(__EMSCRIPTEN__)
    #define PLATFORM PLAT_EMSCR
    #define PLATFLAGS (0)
#elif defined(__FreeBSD__)
    #define PLATFORM PLAT_FREEBSD
    #define PLATFLAGS (PLATFLAG_UNIXLIKE)
#elif defined(__gamecube__)
    #define PLATFORM PLAT_GAMECUBE
    #define PLATFLAGS (0)
#elif defined(PSRC_USEGDK) || defined(_GAMING_DESKTOP) || defined(__GDK__)
    #define PLATFORM PLAT_GDK
    #define PLATFLAGS (PLATFLAG_WINDOWSLIKE)
#elif defined(__HAIKU__)
    #define PLATFORM PLAT_HAIKU
    #define PLATFLAGS (PLATFLAG_UNIXLIKE)
#elif defined(__linux__)
    #define PLATFORM PLAT_LINUX
    #define PLATFLAGS (PLATFLAG_UNIXLIKE)
#elif defined(macintosh) || defined(Macintosh) || (defined(__APPLE__) && defined(__MACH__))
    #define PLATFORM PLAT_MACOS
    #define PLATFLAGS (PLATFLAG_UNIXLIKE)
#elif defined(__NetBSD__)
    #define PLATFORM PLAT_NETBSD
    #define PLATFLAGS (PLATFLAG_UNIXLIKE)
#elif defined(__OpenBSD__)
    #define PLATFORM PLAT_OPENBSD
    #define PLATFLAGS (PLATFLAG_UNIXLIKE)
#elif defined(_EE)
    #define PLATFORM PLAT_PS2
    #define PLATFLAGS (0)
#elif defined(__unix__)
    #define PLATFORM PLAT_UNIX
    #define PLATFLAGS (PLATFLAG_UNIXLIKE)
#elif defined(__wii__)
    #define PLATFORM PLAT_WII
    #define PLATFLAGS (0)
#elif defined(_WIN32) && !(defined(NXDK) || defined(_XBOX))
    #define PLATFORM PLAT_WIN32
    #define PLATFLAGS (PLATFLAG_WINDOWSLIKE)
#elif defined(NXDK)
    #define PLATFORM PLAT_NXDK
    #define PLATFLAGS (PLATFLAG_WINDOWSLIKE)
#else
    #define PLATFORM PLAT_UNKNOWN
    #define PLATFLAGS (0)
    #ifndef _MSC_VER
        #warning Unknown or unsupported platform. \
        This will probably result in a broken build.
    #else
        #pragma message("Unknown or unsupported platform")
    #endif
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || (defined(_M_AMD64) && !defined(_M_ARM64EC))
    #define ARCH ARCH_AMD64
    #define ARCHSTR "x86 64-bit"
    #ifndef BYTEORDER
        #define BYTEORDER BO_LE
    #endif
#elif (defined(__arm__) && !defined(__aarch64__)) || defined(_M_ARM)
    #define ARCH ARCH_ARM
    #define ARCHSTR "Arm"
    #ifndef BYTEORDER
        #if defined(__ARMEL__)
            #define BYTEORDER BO_LE
        #elif defined(__ARMEB__)
            #define BYTEORDER BO_BE
        #endif
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
    #define ARCH ARCH_ARM64
    #define ARCHSTR "Arm 64-bit"
    #ifndef BYTEORDER
        #if defined(__AARCH64EL__)
            #define BYTEORDER BO_LE
        #elif defined(__AARCH64EB__)
            #define BYTEORDER BO_BE
        #endif
    #endif
#elif defined(__i386__) || defined(__i386) || defined(_M_IX86)
    #define ARCH ARCH_I386
    #define ARCHSTR "x86"
    #ifndef BYTEORDER
        #define BYTEORDER BO_LE
    #endif
#elif defined(mips) || defined(__mips) || defined(__mips__)
    #define ARCH ARCH_MIPS
    #define ARCHSTR "MIPS"
#elif defined(__powerpc) || defined(__powerpc__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__)
    #define ARCH ARCH_PPC
    #define ARCHSTR "PowerPC"
#elif defined(__sh__) || defined(__SH__)
    #define ARCH ARCH_SUPERH
    #define ARCHSTR "SuperH"
#elif defined(__wasm) || defined(__wasm__) || defined(__WASM__)
    #define ARCH ARCH_WASM
    #define ARCHSTR "WebAssembly"
#else
    #define ARCH ARCH_UNKNOWN
    #define ARCHSTR "Unknown"
    #ifndef _MSC_VER
        #warning Unknown or unsupported architecture. \
        This may result in a broken build.
    #else
        #pragma message("Unknown or unsupported architecture")
    #endif
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
        #ifndef _MSC_VER
            #warning Unknown or unsupported byte order. \
            This will probably result in a broken build.
        #else
            #pragma message("Unknown or unsupported byte order")
        #endif
    #endif
#endif

extern const char* platname[PLAT__COUNT];
extern const char* platid[PLAT__COUNT];
extern const char* platdir[PLAT__COUNT];
extern const char* altplatdir[PLAT__COUNT];

#endif
