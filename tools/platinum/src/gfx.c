#include "gfx.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
    #include <unistd.h>
    #include <signal.h>
    #include <sys/ioctl.h>
#endif

struct gfx gfx = {
    .redraw = true,
    #ifndef _WIN32
    .charset = GFX_CHARSET_UTF8,
    #else
    .charset = GFX_CHARSET_CP437,
    #endif
    .mouse = {.x = -1, .y = -1},
    .oldmouse = {.x = -1, .y = -1}
};

static void gfx_updatescreensize() {
    #ifndef _WIN32
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    gfx.screen.neww = w.ws_col;
    gfx.screen.newh = w.ws_row;
    #endif
    gfx.resize = true;
}
void gfx_setup(void) {
    gfx_updatescreensize();
    #ifndef _WIN32
    signal(SIGWINCH, gfx_updatescreensize);
    #endif
}
void gfx_cleanup(void) {
    #ifndef _WIN32
    static char* s = "\e[0m";
    write(STDOUT_FILENO, s, strlen(s));
    #endif
}

bool gfx_getregionredraw(int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2; ++y) {
        if (gfx.screen.linechange[y].l >= 0 &&
            x1 >= gfx.screen.linechange[y].l &&
            x2 <= gfx.screen.linechange[y].r) return true;
    }
    return false;
}
static inline void gfx_screen__setchangedsingle(int y, int x) {
    if (gfx.screen.linechange[y].l < 0) {
        gfx.screen.linechange[y].l = x;
        gfx.screen.linechange[y].r = x;
    } else if (x < gfx.screen.linechange[y].l) {
        gfx.screen.linechange[y].l = x;
    } else if (x > gfx.screen.linechange[y].r) {
        gfx.screen.linechange[y].r = x;
    }
}
static inline void gfx_screen__setchangedrange(int y, int l, int r) {
    if (gfx.screen.linechange[y].l < 0) {
        gfx.screen.linechange[y].l = l;
        gfx.screen.linechange[y].r = r;
    } else {
        if (l < gfx.screen.linechange[y].l) {
            gfx.screen.linechange[y].l = l;
        }
        if (r > gfx.screen.linechange[y].r) {
            gfx.screen.linechange[y].r = r;
        }
    }
}

void gfx_setlineredraw(int y) {
    gfx_screen__setchangedrange(y, 0, gfx.screen.w - 1);
}
void gfx_setlinesredraw(int y1, int y2) {
    for (int y = y1; y <= y2; ++y) {
        gfx_screen__setchangedrange(y, 0, gfx.screen.w - 1);
    }
}
void gfx_setregionredraw(int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2; ++y) {
        gfx_screen__setchangedrange(y, x1, x2);
    }
}

void gfx_update(void (*draw)(void)) {
    if (gfx.resize) {
        gfx.resize = false;
        gfx.redraw = true;
        gfx.screen.w = gfx.screen.neww;
        gfx.screen.h = gfx.screen.newh;
        gfx.oldmouse.x = -1;
        gfx.oldmouse.y = -1;
        free(gfx.screen.data);
        int sz = gfx.screen.w * gfx.screen.h;
        gfx.screen.data = malloc(sz * sizeof(*gfx.screen.data));
        for (int i = 0; i < sz; ++i) {
            gfx.screen.data[i] = (struct gfx_char){
                .fgc = GFX_COLOR_WHITE,
                .bgc = GFX_COLOR_BLACK,
                .chr = ' '
            };
        }
        free(gfx.screen.linechange);
        gfx.screen.linechange = malloc(gfx.screen.h * sizeof(*gfx.screen.linechange));
        for (int i = 0; i < gfx.screen.h; ++i) {
            gfx.screen.linechange[i] = (struct gfx_linechange){.l = 0, .r = gfx.screen.w - 1};
        }
    }
    if (!gfx.redraw) return;
    gfx.redraw = false;
    draw();
    if (gfx.mouse.x != gfx.oldmouse.x || gfx.mouse.y != gfx.oldmouse.y) {
        if (gfx.oldmouse.x >= 0 && gfx.oldmouse.y >= 0 && gfx.oldmouse.x < gfx.screen.w && gfx.oldmouse.y < gfx.screen.h) {
            gfx.screen.data[gfx.oldmouse.x + gfx.oldmouse.y * gfx.screen.w].mouse = 0;
            gfx_screen__setchangedsingle(gfx.oldmouse.y, gfx.oldmouse.x);
        }
        if (gfx.mouse.x >= 0 && gfx.mouse.y >= 0 && gfx.mouse.x < gfx.screen.w && gfx.mouse.y < gfx.screen.h) {
            gfx.screen.data[gfx.mouse.x + gfx.mouse.y * gfx.screen.w].mouse = 1;
            gfx_screen__setchangedsingle(gfx.mouse.y, gfx.mouse.x);
        }
        gfx.oldmouse.x = gfx.mouse.x;
        gfx.oldmouse.y = gfx.mouse.y;
    } else if (gfx.mouse.x >= 0 && gfx.mouse.y >= 0 && gfx.mouse.x < gfx.screen.w && gfx.mouse.y < gfx.screen.h &&
    gfx_getregionredraw(gfx.mouse.x, gfx.mouse.y, gfx.mouse.x, gfx.mouse.y)) {
        gfx.screen.data[gfx.mouse.x + gfx.mouse.y * gfx.screen.w].mouse = 1;
        gfx_screen__setchangedsingle(gfx.mouse.y, gfx.mouse.x);
    }
    for (int y = 0; y < gfx.screen.h; ++y) {
        int l = gfx.screen.linechange[y].l;
        if (l < 0) continue;
        gfx.screen.linechange[y].l = -1;
        int r = gfx.screen.linechange[y].r;
        printf("\e[%d;%dH", y + 1, l + 1);
        int off = l + y * gfx.screen.w;
        int fgc = -1, bgc = -1;
        for (int x = l; x <= r; ++x, ++off) {
            // TODO: manually encode based on charset
            struct gfx_char c = gfx.screen.data[off];
            if (c.mouse) {
                int tmp = c.fgc;
                c.fgc = c.bgc;
                c.bgc = tmp;
            }
            if (c.fgc != fgc) {
                fgc = c.fgc;
                if (c.bgc == bgc) {
                    printf("\e[38;5;%dm", fgc);
                } else {
                    bgc = c.bgc;
                    printf("\e[38;5;%d;48;5;%dm", fgc, bgc);
                }
            } else if (c.bgc != bgc) {
                bgc = c.bgc;
                printf("\e[48;5;%dm", bgc);
            }
            fputc(c.chr, stdout);
        }
        fflush(stdout);
    }
}

void gfx_draw_bar(unsigned y, uint8_t fgc, uint8_t bgc, char* text, int len, unsigned ml, unsigned mr, unsigned flags) {
    if (y >= (unsigned)gfx.screen.h) return;
    gfx_screen__setchangedrange(y, 0, gfx.screen.w - 1);
    if (len < 0) len += strlen(text) + 1;
    int off = y * gfx.screen.w;
    if (len > gfx.screen.w) len = gfx.screen.w;
    if (len == gfx.screen.w && !ml && !mr) {
        for (int i = 0; i < gfx.screen.w; ++i, ++off) {
            gfx.screen.data[off] = (struct gfx_char){.chr = text[i], .fgc = fgc, .bgc = bgc};
        }
    } else {
        if (ml + mr >= (unsigned)gfx.screen.w) {
            for (int i = 0; i < gfx.screen.w; ++i, ++off) {
                gfx.screen.data[off] = (struct gfx_char){.chr = ' ', .fgc = fgc, .bgc = bgc};
            }
        } else if (flags & GFX_DRAW_BAR_CENTERTEXT) {
            int sl, sr;
            {
                int tmp = gfx.screen.w - ml - mr;
                if (len > tmp) {
                    len = tmp;
                    sl = ml;
                    sr = mr;
                } else {
                    tmp -= len;
                    sl = tmp / 2 + ml;
                    sr = (tmp + 1) / 2 + mr;
                }
            }
            for (int i = 0; i < sl; ++i, ++off) {
                gfx.screen.data[off] = (struct gfx_char){.chr = ' ', .fgc = fgc, .bgc = bgc};
            }
            for (int i = 0; i < len; ++i, ++off) {
                gfx.screen.data[off] = (struct gfx_char){.chr = text[i], .fgc = fgc, .bgc = bgc};
            }
            for (int i = 0; i < sr; ++i, ++off) {
                gfx.screen.data[off] = (struct gfx_char){.chr = ' ', .fgc = fgc, .bgc = bgc};
            }
        } else {
            int tmp = gfx.screen.w - ml - mr;
            if (len > tmp) len = tmp;
            tmp += mr;
            for (unsigned i = 0; i < ml; ++i, ++off) {
                gfx.screen.data[off] = (struct gfx_char){.chr = ' ', .fgc = fgc, .bgc = bgc};
            }
            int i = 0;
            for (; i < len; ++i, ++off) {
                gfx.screen.data[off] = (struct gfx_char){.chr = text[i], .fgc = fgc, .bgc = bgc};
            }
            for (; i < tmp; ++i, ++off) {
                gfx.screen.data[off] = (struct gfx_char){.chr = ' ', .fgc = fgc, .bgc = bgc};
            }
        }
    }
}
