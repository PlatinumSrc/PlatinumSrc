#define VER_MAJOR 0
#define VER_MINOR 0
#define VER_PATCH 0

#include "tty.h"
#include "input.h"
#include "gfx.h"
#include "time.h"
#include "ptimer.h"
//#include "string.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

static char* filename = NULL;
static FILE* filedata = NULL;

static struct {
    char* name;
    char* author;
    char* comments;
    float bpm;
} song = {
    .bpm = 150.0
};

uint8_t titlecolors[] = {
    GFX_COLOR_BLACK,
    GFX_COLOR_BLACK,
    GFX_COLOR_BLUE,
    GFX_COLOR_BRBLUE,
    GFX_COLOR_BRWHITE,
    GFX_COLOR_BRWHITE,
    GFX_COLOR_BRBLUE,
    GFX_COLOR_BLUE
};
int titlecolor = 0;
#define TITLECOLORS_COUNT (sizeof(titlecolors) / sizeof(*titlecolors))
static void draw(void) {
//    if (gfx.screen.w < 40 || gfx.screen.h < 20) {
//        //gfx_draw_text(0, 0, GFX_COLOR_WHITE, GFX_COLOR_RED, "TERMINAL TOO SMALL");
//        return;
//    }
    if (gfx_getregionbyszredraw(0, 2, gfx.screen.w, gfx.screen.h - 3)) {
        gfx_draw_box(0, 2, gfx.screen.w, gfx.screen.h - 3, GFX_NCOLOR(WHITE, BLUE), 0, GFX_DRAW_BOX_NOBORDER);
    }
    if (gfx_getlineredraw(0)) {
        gfx_draw_bar(0, GFX_COLOR(titlecolors[titlecolor], GFX_COLOR_WHITE), "Platinum Music Tracker", -1, 1, 1, GFX_DRAW_BAR_CENTERTEXT);
    }
    if (gfx_getlineredraw(1)) {
        gfx_draw_bar(1, GFX_NCOLOR(BLACK, WHITE), " File  Edit  Song  Pattern  Track  Window  Help ", -1, 1, 1, 0);
    }
    if (gfx_getlineredraw(gfx.screen.h - 1)) {
        gfx_draw_bar(gfx.screen.h - 1, GFX_NCOLOR(BLACK, WHITE), "Pattern editor - Idle", -1, 1, 1, 0);
    }
    if (gfx_getregionbyszredraw(26, 11, 32, 11)) {
        unsigned bdrf = rand() & GFX_DRAW_BOX_DBLBORDER;
        gfx_draw_box(26, 11, 30, 10, rand(), rand(), GFX_DRAW_BOX_SHADOW | bdrf);
    }
    if (gfx_getregionbyszredraw(17, 14, 32, 11)) {
        unsigned bdrf = rand() & GFX_DRAW_BOX_DBLBORDER;
        gfx_draw_box(17, 14, 30, 10, rand(), rand(), GFX_DRAW_BOX_SHADOW | bdrf);
    }
    if (gfx_getregionbyszredraw(20, 8, 32, 11)) {
        unsigned bdrf = rand() & GFX_DRAW_BOX_DBLBORDER;
        gfx_draw_box(20, 8, 30, 10, rand(), rand(), GFX_DRAW_BOX_SHADOW | bdrf);
    }
}

static void run(void) {
    enum __attribute__((packed)) ev {
        EV_NOTHING,
        EV_TITLE
    };
    ptimer_new(500000, (void*)EV_TITLE);
    while (1) {
        uint64_t tt;
        void* td;
        int ti = ptimer_getnext(&tt, &td);
        recalctimer:;
        int selret = (ti >= 0) ? tty_waituntil(tt) : tty_pause();
        if (selret > 0) kbuf_update();
        struct key k;
        while (getkey(&k)) {
            switch (k.t & KT_MASK) {
                case KT_CHAR: {
                    if ((k.t & KF_MASK) == KF_CTRL && k.c == 'q') return;
                } break;
                case KT_MOUSEMOVE: {
                    gfx.mouse.x = k.mx; gfx.mouse.y = k.my;
                    gfx.redraw = true;
                }
            }
            #if 0
            printf("k.t: [S%uC%uA%u %2u] (0x%02X)", (k.t >> 5) & 1, (k.t >> 6) & 1, (k.t >> 7) & 1, k.t & KT_MASK, k.t);
            switch (k.t & KT_MASK) {
                case KT_CHAR: printf(", k.c: %d (0x%02X)", k.c, k.c); break;
                case KT_SP: printf(", k.sp: %u", k.sp); break;
                case KT_NAV: printf(", k.nav: %u", k.nav); break;
                case KT_FN: printf(", k.fn: %u", k.fn); break;
                case KT_MOUSEMOVE: gfx.mouse.x = k.mx; gfx.mouse.y = k.my; printf(", k.m[x,y]: [%u, %u]", k.mx, k.my); break;
                case KT_MOUSEBUTTON: printf(", k.m[bdn,b]: [%u, %u]", k.mbdn, k.mb); break;
                case KT_MOUSESCROLL: printf(", k.ms: %d", k.ms); break;
            }
            putchar('\n');
            #endif
        }
        if (!selret && ti >= 0) {
            switch ((enum ev)td) {
                default: break;
                case EV_TITLE: {
                    titlecolor = (titlecolor + 1) % TITLECOLORS_COUNT;
                    gfx_setlineredraw(0);
                    gfx.redraw = true;
                } break;
            };
        }
        gfx_update(draw);
        if (selret) goto recalctimer;
    }
}

static inline void setup() {
    srand(time(NULL) ^ altutime());
    tty_setup();
    gfx_setup();
    ptimer_init();
    gfx_update(draw);
}
static inline void cleanup() {
    gfx_cleanup();
    tty_cleanup();
}

static void sigh(int sig) {
    #if defined(SIGABRT)
    if (sig == SIGABRT) {
        char c;
        read(0, &c, 1);
    }
    #endif
    cleanup();
    exit(0);
}
static void die(const char* argv0, const char* f, ...) {
    fprintf(stderr, "%s: ", argv0);
    va_list v;
    va_start(v, f);
    vfprintf(stderr, f, v);
    va_end(v);
    putchar('\n');
    exit(1);
}
static void puthelp(const char* argv0) {
    printf("Platinum Tracker version %d.%d.%d\n", VER_MAJOR, VER_MINOR, VER_PATCH);
    printf("\nUSAGE: %s [OPTION]... FILE\n", argv0);
    puts("\nOPTIONS:");
    puts("      --help        Display help text and exit");
    puts("      --version     Display version info and exit");
    puts("  -c, --charset     Character set to use");
    puts("                        ascii: 7-bit ASCII");
    #ifndef _WIN32
    puts("                        cp437: 8-bit code page 437");
    puts("                        utf-8: UTF-8 (default)");
    #else
    puts("                        cp437: 8-bit code page 437 (default)");
    puts("                        utf-8: UTF-8");
    #endif
    exit(0);
}
static void putver(void) {
    printf("Platinum Tracker version %d.%d.%d\n\n", VER_MAJOR, VER_MINOR, VER_PATCH);
    puts("Made by PQCraft as part of the PlatinumSrc project");
    puts("<https://github.com/PQCraft/PlatinumSrc>\n");
    puts("Licensed under the Boost Software License 1.0");
    puts("<https://www.boost.org/LICENSE_1_0.txt>");
    exit(0);
}
int main(int argc, char** argv) {
    {
        bool nomore = false;
        for (int i = 1; i < argc; ++i) {
            if (!nomore && argv[i][0] == '-' && argv[i][1]) {
                bool shortopt;
                if (argv[i][1] == '-') {
                    shortopt = false;
                    if (!argv[i][2]) {nomore = true; continue;}
                } else {
                    shortopt = true;
                }
                char* opt = argv[i];
                char sopt = 0;
                int sopos = 0;
                soret:;
                ++sopos;
                char* optname;
                char soptname[] = {'-', 0, 0};
                if (shortopt) {
                    if (!(sopt = opt[sopos])) continue;
                    soptname[1] = sopt;
                    optname = soptname;
                } else {
                    optname = opt;
                    opt += 2;
                }
                if (!shortopt && !strcmp(opt, "help")) {
                    puthelp(argv[0]);
                } else if (!shortopt && !strcmp(opt, "version")) {
                    putver();
                } else if ((shortopt) ? sopt == 'c' : !strcmp(opt, "charset")) {
                    ++i;
                    if (i >= argc) die(argv[0], "Expected argument value for %s", optname);
                    if (!strcmp(argv[i], "ascii")) {
                         gfx.charset = GFX_CHARSET_ASCII;
                    } else if (!strcmp(argv[i], "cp437") || !strcmp(argv[i], "437")) {
                         gfx.charset = GFX_CHARSET_CP437;
                    } else if (!strcmp(argv[i], "utf-8") || !strcmp(argv[i], "utf8") || !strcmp(argv[i], "utf")) {
                         gfx.charset = GFX_CHARSET_UTF8;
                    } else {
                        die(argv[0], "Invalid value for argument %s: %s", optname, argv[i]);
                    }
                } else {
                    die(argv[0], "Unknown argument: %s", (shortopt) ? (char[]){'-', sopt, 0} : opt - 2);
                }
                if (shortopt) goto soret;
            } else {
                if (filename) die(argv[0], "File already provided %s");
                filename = strdup(argv[i]);
            }
        }
        if (filename) {
            filedata = fopen(filename, "rb+");
        } else {
            filename = strdup("untitled.ptm");
        }
    }
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) die(argv[0], "Not running under a tty");
    signal(SIGINT, sigh);
    signal(SIGTERM, sigh);
    #ifdef SIGQUIT
    signal(SIGQUIT, sigh);
    #endif
    #ifdef SIGABRT
    signal(SIGABRT, sigh);
    #endif
    #ifdef SIGHUP
    signal(SIGHUP, sigh);
    #endif
    #ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    #endif
    #ifdef SIGUSR1
    signal(SIGUSR1, SIG_IGN);
    #endif
    #ifdef SIGUSR2
    signal(SIGUSR2, SIG_IGN);
    #endif
    setup();
    run();
    cleanup();
    return 0;
}
