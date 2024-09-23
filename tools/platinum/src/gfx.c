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
    if (x1 >= gfx.screen.w || y1 >= gfx.screen.h || x2 < 0 || y2 < 0) return false;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= gfx.screen.w) x2 = gfx.screen.w - 1;
    if (y2 >= gfx.screen.h) y2 = gfx.screen.h - 1;
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
static inline void gfx_screen__setchangedregion(int y1, int y2, int l, int r) {
    for (int y = y1; y <= y2; ++y) {
        gfx_screen__setchangedrange(y, l, r);
    }
}

void gfx_setlineredraw(int y) {
    gfx_screen__setchangedrange(y, 0, gfx.screen.w - 1);
}
void gfx_setlinesredraw(int y1, int y2) {
    gfx_screen__setchangedregion(y1, y2, 0, gfx.screen.w - 1);
}
void gfx_setregionredraw(int x1, int y1, int x2, int y2) {
    gfx_screen__setchangedregion(y1, y2, x1, x2);
}

static int gfx_putc_ascii(int c) {
    static const char l[] = {
        0, '?', '?', '?', '?', '?', '?', '*', '?', 'o', '?', '?', '?', '?', '?', '?',
        '>', '<', ':', '!', '?', '?', '_', ';', '^', 'v', '>', '<', 'L', '-', '^', 'v'
    };
    static const char h[] = {
        'C', 'u', 'e', 'a', 'a', 'a', 'a', 'c', 'e', 'e', 'e', 'i', 'i', 'i', 'A', 'A',
        'E', 'a', 'A', 'o', 'o', 'o', 'u', 'u', 'y', 'O', 'U', 'c', 'E', 'Y', 'P', 'f',
        'a', 'i', 'o', 'u', 'n', 'N', 'a', 'o', '?', '-', '-', '2', '4', 'i', '<', '>',
        '#', '#', '#', '|', '|', '|', '#', ',', '.', '#', '#', ',', '\'', '\'', '\'', '.',
        '`', '\'', '.', '|', '-', '+', '|', '#', '`', ',', '\'', ',', '#', '=', '#', '\'',
        '"', '.', ',', '`', '`', '.', ',', '#', '+', '\'', '.', '#', '#', '#', '#', '#',
        'a', 'B', '|', '?', 'E', 'o', 'u', '?', '?', 'O', '?', '?', '?', '?', 'E', 'n',
        '=', '+', '>', '<', '|', '|', '?', '=', 'o', '.', '.', '?', 'n', '2', '#', '?'
    };
    switch ((uint8_t)c) {
        default: return putchar(c);
        case 0x00 ... 0x1F: return putchar(l[c]);
        case 0x80 ... 0xFF: return putchar(h[c & 0x7F]);
    }
}
static int gfx_putc_utf8(int c) {
    static const uint16_t l[] = {
        0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, 
        0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C, 
        0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8, 
        0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC
    };
    static const uint16_t h[] = {
        0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 
        0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5, 
        0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 
        0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192, 
        0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 
        0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB, 
        0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 
        0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510, 
        0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 
        0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567, 
        0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 
        0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580, 
        0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 
        0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229, 
        0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 
        0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
    };
    switch ((uint8_t)c) {
        default: return putchar(c);
        case 0x00 ... 0x1F: {
            uint16_t tmp = l[c];
            if (tmp < 0x0800) {
                putchar(0xC0 | (tmp >> 6 & 0x1F));
            } else {
                putchar(0xE0 | (tmp >> 12 & 0x0F));
                putchar(0x80 | (tmp >> 6 & 0x3F));
            }
            return putchar(0x80 | (tmp & 0x3F));
        }
        case 0x80 ... 0xFF: {
            uint16_t tmp = h[c & 0x7F];
            if (tmp < 0x0800) {
                putchar(0xC0 | (tmp >> 6 & 0x1F));
            } else {
                putchar(0xE0 | (tmp >> 12 & 0x0F));
                putchar(0x80 | (tmp >> 6 & 0x3F));
            }
            return putchar(0x80 | (tmp & 0x3F));
        }
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
        uint16_t tmpc = GFX_CHAR(' ', GFX_NCOLOR(WHITE, BLACK));
        for (int i = 0; i < sz; ++i) {
            gfx.screen.data[i] = tmpc;
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
            gfx_screen__setchangedsingle(gfx.oldmouse.y, gfx.oldmouse.x);
        }
        if (gfx.mouse.x >= 0 && gfx.mouse.y >= 0 && gfx.mouse.x < gfx.screen.w && gfx.mouse.y < gfx.screen.h) {
            gfx_screen__setchangedsingle(gfx.mouse.y, gfx.mouse.x);
        }
        gfx.oldmouse.x = gfx.mouse.x;
        gfx.oldmouse.y = gfx.mouse.y;
    } else if (gfx.mouse.x >= 0 && gfx.mouse.y >= 0 && gfx.mouse.x < gfx.screen.w && gfx.mouse.y < gfx.screen.h &&
    gfx_getregionredraw(gfx.mouse.x, gfx.mouse.y, gfx.mouse.x, gfx.mouse.y)) {
        gfx_screen__setchangedsingle(gfx.mouse.y, gfx.mouse.x);
    }
    int fgc = -1, bgc = -1;
    int moff = -1;
    int (*putcharptr)(int);
    switch (gfx.charset) {
        case GFX_CHARSET_ASCII: putcharptr = gfx_putc_ascii; break;
        case GFX_CHARSET_CP437: putcharptr = putchar; break;
        case GFX_CHARSET_UTF8: putcharptr = gfx_putc_utf8; break;
    }
    for (int y = 0; y < gfx.screen.h; ++y) {
        int l = gfx.screen.linechange[y].l;
        if (l < 0) continue;
        if (l >= gfx.screen.w) l = gfx.screen.w - 1;
        gfx.screen.linechange[y].l = -1;
        int r = gfx.screen.linechange[y].r;
        if (r >= gfx.screen.w) r = gfx.screen.w - 1;
        printf("\e[%d;%dH", y + 1, l + 1);
        int off = l + y * gfx.screen.w;
        int end = r + y * gfx.screen.w;
        if (gfx.mouse.y == y && gfx.mouse.x >= 0 && gfx.mouse.x < gfx.screen.w) {
            if (gfx.mouse.x < l) {
                l = gfx.mouse.x;
            } else if (gfx.mouse.x > r) {
                r = gfx.mouse.x;
            }
            moff = gfx.mouse.x + gfx.mouse.y * gfx.screen.w;
        }
        for (; off <= end; ++off) {
            uint16_t c = gfx.screen.data[off];
            uint8_t cfgc = GFX_CHAR_GETFGC(c);
            uint8_t cbgc = GFX_CHAR_GETBGC(c);
            if (off == moff) {
                cfgc &= 7;
                cfgc ^= 7;
                cbgc &= 7;
                cbgc ^= 7;
            }
            if (cbgc != bgc) {
                bgc = cbgc;
                putchar('\e');
                putchar('[');
                if (cfgc != fgc) {
                    fgc = cfgc;
                    printf("%d;", fgc + (!(fgc & GFX_COLOR_BRIGHT) ? 30 : 82));
                }
                /*if (!gfx.bghack) */printf("%dm", bgc + (!(bgc & GFX_COLOR_BRIGHT) ? 40 : 92));
                //else printf("48;5;%dm", bgc);
            } else if (cfgc != fgc) {
                fgc = cfgc;
                printf("\e[%dm", fgc + (!(fgc & GFX_COLOR_BRIGHT) ? 30 : 82));
            }
            putcharptr(GFX_CHAR_GETCHAR(c));
            // TODO: change depending on charset
        }
    }
    fflush(stdout);
}

static void gfx_drawprim_box(int x1, int y1, int x2, int y2, uint8_t color) {
    for (; y1 <= y2; ++y1) {
        int off = x1 + y1 * gfx.screen.w;
        int end = x2 + y1 * gfx.screen.w;
        uint16_t tmpc = GFX_CHAR(' ', color);
        for (; off <= end; ++off) {
            gfx.screen.data[off] = tmpc;
        }
    }
}
static void gfx_drawprim_border(int x1, int y1, int x2, int y2, uint8_t color, uint8_t t, uint8_t b, uint8_t l, uint8_t r) {
    enum __attribute__((packed)) {
        BC_TL,   BC_TM,   BC_TR,
        BC_ML, /*BC_MM,*/ BC_MR,
        BC_BL,   BC_BM,   BC_BR,
        BC__COUNT
    };
    uint8_t bc[BC__COUNT];
    if (t == 1) {
        bc[BC_TM] = 0xC4;
        if (l == 1) bc[BC_TL] = 0xDA;
        else if (l == 2) bc[BC_TL] = 0xD6;
        if (r == 1) bc[BC_TR] = 0xBF;
        else if (r == 2) bc[BC_TR] = 0xB7;
    } else if (t == 2) {
        bc[BC_TM] = 0xCD;
        if (l == 1) bc[BC_TL] = 0xD5;
        else if (l == 2) bc[BC_TL] = 0xC9;
        if (r == 1) bc[BC_TR] = 0xB8;
        else if (r == 2) bc[BC_TR] = 0xBB;
    }
    if (b == 1) {
        bc[BC_BM] = 0xC4;
        if (l == 1) bc[BC_BL] = 0xC0;
        else if (l == 2) bc[BC_BL] = 0xD3;
        if (r == 1) bc[BC_BR] = 0xD9;
        else if (r == 2) bc[BC_BR] = 0xBD;
    } else if (b == 2) {
        bc[BC_BM] = 0xCD;
        if (l == 1) bc[BC_BL] = 0xD4;
        else if (l == 2) bc[BC_BL] = 0xC8;
        if (r == 1) bc[BC_BR] = 0xBE;
        else if (r == 2) bc[BC_BR] = 0xBC;
    }
    if (l == 1) bc[BC_ML] = 0xB3;
    else if (l == 2) bc[BC_ML] = 0xBA;
    if (r == 1) bc[BC_MR] = 0xB3;
    else if (r == 2) bc[BC_MR] = 0xBA;
    uint16_t tmpc = GFX_CHAR(0, color);
    if (t) {
        int off = x1 + y1 * gfx.screen.w;
        int end = x2 + y1 * gfx.screen.w;
        ++y1;
        if (l) gfx.screen.data[off++] = bc[BC_TL] | tmpc;
        if (r) gfx.screen.data[end--] = bc[BC_TR] | tmpc;
        for (; off <= end; ++off) {
            gfx.screen.data[off] = bc[BC_TM] | tmpc;
        }
    }
    if (b) {
        int off = x1 + y2 * gfx.screen.w;
        int end = x2 + y2 * gfx.screen.w;
        --y2;
        if (l) gfx.screen.data[off++] = bc[BC_BL] | tmpc;
        if (r) gfx.screen.data[end--] = bc[BC_BR] | tmpc;
        for (; off <= end; ++off) {
            gfx.screen.data[off] = bc[BC_BM] | tmpc;
        }
    }
    int off = y1 * gfx.screen.w;
    int end = y2 * gfx.screen.w;
    for (; off <= end; off += gfx.screen.w) {
        if (l) gfx.screen.data[off + x1] = bc[BC_ML] | tmpc;
        if (r) gfx.screen.data[off + x2] = bc[BC_MR] | tmpc;
    }
}
static void gfx_drawprim_shadow(int x1, int y1, int x2, int y2) {
    for (; y1 <= y2; ++y1) {
        int off = x1 + y1 * gfx.screen.w;
        int end = x2 + y1 * gfx.screen.w;
        uint16_t tmpc = GFX_CHAR(0, GFX_NCOLOR(BRBLACK, BLACK));
        for (; off <= end; ++off) {
            gfx.screen.data[off] = GFX_CHAR_GETCHAR(gfx.screen.data[off]) | tmpc;
        }
    }
}

void gfx_draw_bar(unsigned y, uint8_t color, char* text, int len, unsigned ml, unsigned mr, unsigned f) {
    if (y >= (unsigned)gfx.screen.h) return;
    if (!(f & GFX_DRAW_NOMARKDRAWN)) gfx_screen__setchangedrange(y, 0, gfx.screen.w - 1);
    if (len < 0) len += strlen(text) + 1;
    int off = y * gfx.screen.w;
    if (len > gfx.screen.w) len = gfx.screen.w;
    uint16_t colorbits = GFX_CHAR(0, color);
    uint16_t tmpc = ' ' | colorbits;
    if (len == gfx.screen.w && !ml && !mr) {
        for (int i = 0; i < gfx.screen.w; ++i, ++off) {
            gfx.screen.data[off] = text[i] | colorbits;
        }
    } else {
        if (ml + mr >= (unsigned)gfx.screen.w) {
            for (int i = 0; i < gfx.screen.w; ++i, ++off) {
                gfx.screen.data[off] = tmpc;
            }
        } else if (f & GFX_DRAW_BAR_CENTERTEXT) {
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
                gfx.screen.data[off] = tmpc;
            }
            for (int i = 0; i < len; ++i, ++off) {
                gfx.screen.data[off] = text[i] | colorbits;
            }
            for (int i = 0; i < sr; ++i, ++off) {
                gfx.screen.data[off] = tmpc;
            }
        } else {
            int tmp = gfx.screen.w - ml - mr;
            if (len > tmp) len = tmp;
            tmp += mr;
            for (unsigned i = 0; i < ml; ++i, ++off) {
                gfx.screen.data[off] = tmpc;
            }
            int i = 0;
            for (; i < len; ++i, ++off) {
                gfx.screen.data[off] = text[i] | colorbits;
            }
            for (; i < tmp; ++i, ++off) {
                gfx.screen.data[off] = tmpc;
            }
        }
    }
}
void gfx_draw_box(int x1, int y1, int x2, int y2, uint8_t fc, uint8_t bc, unsigned f) {
    if (x2 < 0) {
        x2 += gfx.screen.w;
        if (x2 < x1) return;
    } else {
        x2 += x1 - 1;
    }
    if (y2 < 0) {
        y2 += gfx.screen.h;
        if (y2 < y1) return;
    } else {
        y2 += y1 - 1;
    }
    if (x1 >= gfx.screen.w || y1 >= gfx.screen.h) return;
    if (x2 >= 0 && y2 >= 0) {
        int bcx1 = x1, bcy1 = y1, bcx2 = x2, bcy2 = y2;
        if (!(f & GFX_DRAW_BOX_NOBORDER)) {
            uint8_t bt, bb, bl, br;
            if (bcx1 < 0) {bcx1 = 0; bl = 0;}
            else bl = 1 + !!(f & GFX_DRAW_BOX_DBLBORDER_L);
            if (bcy1 < 0) {bcy1 = 0; bt = 0;}
            else bt = 1 + !!(f & GFX_DRAW_BOX_DBLBORDER_T);
            if (bcx2 >= gfx.screen.w) {bcx2 = gfx.screen.w - 1; br = 0;}
            else br = 1 + !!(f & GFX_DRAW_BOX_DBLBORDER_R);
            if (bcy2 >= gfx.screen.h) {bcy2 = gfx.screen.h - 1; bb = 0;}
            else bb = 1 + !!(f & GFX_DRAW_BOX_DBLBORDER_B);
            gfx_drawprim_border(bcx1, bcy1, bcx2, bcy2, bc, bt, bb, bl, br);
            if (bl) ++bcx1;
            if (bt) ++bcy1;
            if (br) --bcx2;
            if (bb) --bcy2;
            if (bcx1 <= bcx2 && bcy1 <= bcy2) {
                gfx_drawprim_box(bcx1, bcy1, bcx2, bcy2, fc);
            }
        } else {
            if (bcx1 < 0) bcx1 = 0;
            if (bcy1 < 0) bcy1 = 0;
            if (bcx2 >= gfx.screen.w) bcx2 = gfx.screen.w - 1;
            if (bcy2 >= gfx.screen.h) bcy2 = gfx.screen.h - 1;
            gfx_drawprim_box(bcx1, bcy1, bcx2, bcy2, fc);
        }
        if (!(f & GFX_DRAW_NOMARKDRAWN)) gfx_screen__setchangedregion(bcy1, bcy2, bcx1, bcx2);
    }
    if (!(f & GFX_DRAW_BOX_SHADOW)) return;
    int bcx1 = x1 + 2, bcy1 = y1 + 1, bcx2 = x2 + 2, bcy2 = y2 + 1;
    if (bcx1 >= gfx.screen.w || bcy1 >= gfx.screen.h || bcx2 < 0 || bcy2 < 0) return;
    if (bcx1 < 0) bcx1 = 0;
    if (bcy1 < 0) bcy1 = 0;
    if (bcx2 >= gfx.screen.w) bcx2 = gfx.screen.w - 1;
    if (bcy2 >= gfx.screen.h) bcy2 = gfx.screen.h - 1;
    if (x2 >= gfx.screen.w) x2 = gfx.screen.w - 1;
    if (bcy2 > y2 && bcx1 <= x2) {
        gfx_drawprim_shadow(bcx1, bcy2, x2, bcy2);
        if (!(f & GFX_DRAW_NOMARKDRAWN)) gfx_screen__setchangedrange(bcy2, bcx1, x2);
    }
    int e = x2 + 1;
    if (e <= bcx2) {
        gfx_drawprim_shadow(e, bcy1, bcx2, bcy2);
        if (!(f & GFX_DRAW_NOMARKDRAWN)) gfx_screen__setchangedregion(bcy1, bcy2, e, bcx2);
    }
}
