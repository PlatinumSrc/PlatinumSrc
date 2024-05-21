#include "platform.h"
#include <stddef.h>

const char* platname[PLAT__COUNT] = {
    "Unknown",
    "3DS",
    "Android",
    "Dreamcast (KallistiOS)",
    "Emscripten",
    "FreeBSD",
    "GameCube",
    "Haiku",
    "Linux",
    "MacOS",
    "NetBSD",
    "OpenBSD",
    "PlayStation 2 (PS2Dev)",
    "Unix",
    "Wii",
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
    "gamecube",
    "haiku",
    "linux",
    "macos",
    "netbsd",
    "openbsd",
    "ps2",
    "unix",
    "wii",
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
    "gamecube",
    "haiku",
    "linux",
    "macos",
    "netbsd",
    "openbsd",
    "ps2",
    "unix",
    "wii",
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
    "gc",
    NULL,
    NULL,
    NULL,
    "bsd",
    "bsd",
    NULL,
    "unix",
    NULL,
    "win32",
    "xbox"
};
