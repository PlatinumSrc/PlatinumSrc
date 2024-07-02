#include "common.h"
#include "platform.h"

#include <stddef.h>
#include <string.h>

#include "glue.h"

int quitreq = 0;

struct options options = {0};

char* maindir = NULL;
char* userdir = NULL;
char* gamedir = NULL;
char* savedir = NULL;

struct cfg* config = NULL;
struct cfg* gameconfig = NULL;

bool common_findpv(const char* n, int* d) {
    if (*n) {
        if (*n == 'm' || *n == 'M') {
            ++n;
            if (!strcasecmp(n, "odule")) {
                #if defined(PSRC_MODULE_SERVER)
                    *d = 1;
                #elif defined(PSRC_MODULE_EDITOR)
                    *d = 2;
                #else
                    *d = 0;
                #endif
                return true;
            } else if (!strcasecmp(n, "odule_client")) {
                *d = 0; return true;
            } else if (!strcasecmp(n, "odule_server")) {
                *d = 1; return true;
            } else if (!strcasecmp(n, "odule_editor")) {
                *d = 2; return true;
            }
        } else if (*n == 'p' || *n == 'P') {
            ++n;
            if (!strcasecmp(n, "latform")) {
                *d = PLATFORM; return true;
            } else if (!strcasecmp(n, "lat_3ds")) {
                *d = PLAT_3DS; return true;
            } else if (!strcasecmp(n, "lat_android")) {
                *d = PLAT_ANDROID; return true;
            } else if (!strcasecmp(n, "lat_dreamcast")) {
                *d = PLAT_DREAMCAST; return true;
            } else if (!strcasecmp(n, "lat_emscr")) {
                *d = PLAT_EMSCR; return true;
            } else if (!strcasecmp(n, "lat_freebsd")) {
                *d = PLAT_FREEBSD; return true;
            } else if (!strcasecmp(n, "lat_gamecube")) {
                *d = PLAT_GAMECUBE; return true;
            } else if (!strcasecmp(n, "lat_haiku")) {
                *d = PLAT_HAIKU; return true;
            } else if (!strcasecmp(n, "lat_linux")) {
                *d = PLAT_LINUX; return true;
            } else if (!strcasecmp(n, "lat_macos")) {
                *d = PLAT_MACOS; return true;
            } else if (!strcasecmp(n, "lat_netbsd")) {
                *d = PLAT_NETBSD; return true;
            } else if (!strcasecmp(n, "lat_openbsd")) {
                *d = PLAT_OPENBSD; return true;
            } else if (!strcasecmp(n, "lat_ps2")) {
                *d = PLAT_PS2; return true;
            } else if (!strcasecmp(n, "lat_unix")) {
                *d = PLAT_UNIX; return true;
            } else if (!strcasecmp(n, "lat_uwp")) {
                *d = PLAT_UWP; return true;
            } else if (!strcasecmp(n, "lat_wii")) {
                *d = PLAT_WII; return true;
            } else if (!strcasecmp(n, "lat_win32")) {
                *d = PLAT_WIN32; return true;
            } else if (!strcasecmp(n, "lat_nxdk")) {
                *d = PLAT_NXDK; return true;
            } else if (!strcasecmp(n, "latflags")) {
                *d = PLATFLAGS; return true;
            } else if (!strcasecmp(n, "latflag_unixlike")) {
                *d = PLATFLAG_UNIXLIKE; return true;
            } else if (!strcasecmp(n, "latflag_windowslike")) {
                *d = PLATFLAG_WINDOWSLIKE; return true;
            }
        }
    }
    return false;
}
