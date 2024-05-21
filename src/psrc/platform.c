#include "platform.h"
#include <stddef.h>

const char* platname[PLAT__COUNT] = {
    "Unknown",
    "3DS",
    "Android",
    "Dreamcast (KallistiOS)",
    "Emscripten",
    "FreeBSD",
    "Haiku",
    "Linux",
    "MacOS",
    "NetBSD",
    "OpenBSD",
    "PlayStation 2 (PS2Dev)",
    "Unix",
    "Windows",
    "Xbox (NXDK)"
};
const char* platid[PLAT__COUNT] = {
    "unknown",
    "3ds",
    "android",
    "dreamcast",
    "emscr",
    "freebsd",
    "haiku",
    "linux",
    "macos",
    "netbsd",
    "openbsd",
    "ps2",
    "unix",
    "win32",
    "nxdk"
};
const char* platdir[PLAT__COUNT] = {
    "unknown",
    "3ds",
    "android",
    "dreamcast",
    "emscripten",
    "freebsd",
    "haiku",
    "linux",
    "macos",
    "netbsd",
    "openbsd",
    "ps2",
    "unix",
    "windows",
    "nxdk"
};
const char* altplatdir[PLAT__COUNT] = {
    NULL,
    NULL,
    NULL,
    "dc",
    "emscr",
    "bsd",
    NULL,
    NULL,
    NULL,
    "bsd",
    "bsd",
    NULL,
    "unix",
    "win32",
    "xbox"
};
