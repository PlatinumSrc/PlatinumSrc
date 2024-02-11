#include "platform.h"
#include <stddef.h>

const char* platname[PLAT__COUNT] = {
    "Unknown",
    "Android",
    "Dreamcast",
    "Emscripten",
    "FreeBSD",
    "Haiku",
    "Linux",
    "MacOS",
    "NetBSD",
    "OpenBSD",
    "PlayStation 2",
    "Unix",
    "Windows",
    "Xbox (NXDK)"
};
const char* platdir[PLAT__COUNT] = {
    "unknown",
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
