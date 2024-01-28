#include "platform.h"
#include <stddef.h>

const char* platname[PLAT__COUNT] = {
    "Unknown",
    "Android",
    "Emscripten",
    "FreeBSD",
    "Haiku",
    "Linux",
    "MacOS",
    "NetBSD",
    "OpenBSD",
    "Unix",
    "Windows",
    "Xbox (NXDK)"
};
const char* platdir[PLAT__COUNT] = {
    "unknown",
    "android",
    "emscripten",
    "freebsd",
    "haiku",
    "linux",
    "macos",
    "netbsd",
    "openbsd",
    "unix",
    "windows",
    "nxdk"
};
const char* altplatdir[PLAT__COUNT] = {
    NULL,
    NULL,
    "emscr",
    "bsd",
    NULL,
    NULL,
    NULL,
    "bsd",
    "bsd",
    "unix",
    "win32",
    "xbox"
};
