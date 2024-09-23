#ifndef PLATINUM_GFX_H
#define PLATINUM_GFX_H

#include <stdint.h>
#include <stdbool.h>

enum __attribute__((packed)) gfx_charset {
    GFX_CHARSET_ASCII,
    GFX_CHARSET_CP437,
    GFX_CHARSET_UTF8
};

#define GFX_COLOR_BLACK     0
#define GFX_COLOR_RED       (1 << 0)
#define GFX_COLOR_GREEN     (1 << 1)
#define GFX_COLOR_YELLOW    (GFX_COLOR_RED | GFX_COLOR_GREEN)
#define GFX_COLOR_BLUE      (1 << 2)
#define GFX_COLOR_MAGENTA   (GFX_COLOR_RED | GFX_COLOR_BLUE)
#define GFX_COLOR_CYAN      (GFX_COLOR_GREEN | GFX_COLOR_BLUE)
#define GFX_COLOR_WHITE     (GFX_COLOR_RED | GFX_COLOR_GREEN | GFX_COLOR_BLUE)
#define GFX_COLOR_BRIGHT    (1 << 3)
#define GFX_COLOR_BRBLACK   GFX_COLOR_BRIGHT
#define GFX_COLOR_BRRED     (GFX_COLOR_RED | GFX_COLOR_BRIGHT)
#define GFX_COLOR_BRGREEN   (GFX_COLOR_GREEN | GFX_COLOR_BRIGHT)
#define GFX_COLOR_BRYELLOW  (GFX_COLOR_YELLOW | GFX_COLOR_BRIGHT)
#define GFX_COLOR_BRBLUE    (GFX_COLOR_BLUE | GFX_COLOR_BRIGHT)
#define GFX_COLOR_BRMAGENTA (GFX_COLOR_MAGENTA | GFX_COLOR_BRIGHT)
#define GFX_COLOR_BRCYAN    (GFX_COLOR_CYAN | GFX_COLOR_BRIGHT)
#define GFX_COLOR_BRWHITE   (GFX_COLOR_WHITE | GFX_COLOR_BRIGHT)
#define GFX_COLOR(f, b)     ((f) | ((b) << 4))
#define GFX_NCOLOR(f, b)    ((GFX_COLOR_##f) | ((GFX_COLOR_##b) << 4))

#define GFX_CHAR_GETCHAR(c)  ((c) & 255)
#define GFX_CHAR_GETCOLOR(c) ((c) >> 8)
#define GFX_CHAR_GETFGC(c)   (((c) >> 8) & 15)
#define GFX_CHAR_GETBGC(c)   ((c) >> 12)

#define GFX_CHAR(ch, co) ((ch) | ((co) << 8))

struct __attribute__((packed)) gfx_linechange {
    int16_t l, r;
};
extern struct gfx {
    struct {
        uint16_t* data;
        int w, h;
        int neww, newh;
        struct gfx_linechange* linechange;
    } screen;
    bool redraw : 1;
    bool resize : 1;
    bool bghack : 1;
    enum gfx_charset charset;
    struct {
        int16_t x, y;
    } mouse, oldmouse;
} gfx;

void gfx_setup(void);
void gfx_cleanup(void);

static inline bool gfx_getlineredraw(int y) {
    if (y < 0 || y >= gfx.screen.h) return false;
    return (gfx.screen.linechange[y].l >= 0);
}
bool gfx_getlinesredraw(int y1, int y2);
bool gfx_getregionredraw(int x1, int y1, int x2, int y2);
static inline bool gfx_getregionbyszredraw(int x, int y, int w, int h) {
    return gfx_getregionredraw(x, y, x + w - 1, y + h - 1);
}

void gfx_setlineredraw(int y);
void gfx_setlinesredraw(int y1, int y2);
void gfx_setregionredraw(int x1, int y1, int x2, int y2);

static inline void gfx_set(int i, uint16_t c) {
    gfx.screen.data[i] = c;
}
static inline uint16_t gfx_get(int i) {
    return gfx.screen.data[i];
}
static inline void gfx_setxy(int x, int y, uint16_t c) {
    gfx.screen.data[x + y * gfx.screen.w] = c;
}
static inline uint16_t gfx_getxy(int x, int y) {
    return gfx.screen.data[x + y * gfx.screen.w];
}

void gfx_update(void (*draw)(void));

#define GFX_DRAW_NOMARKDRAWN (1 << 16)

#define GFX_DRAW_BAR_CENTERTEXT (1 << 0)
void gfx_draw_bar(unsigned y, uint8_t color, char* text, int len, unsigned marginl, unsigned marginr, unsigned flags); // TODO: remove

#define GFX_DRAW_BOX_NOBORDER    (1 << 0)
#define GFX_DRAW_BOX_SHADOW      (1 << 1)
#define GFX_DRAW_BOX_DBLBORDER_T (1 << 2)
#define GFX_DRAW_BOX_DBLBORDER_B (1 << 3)
#define GFX_DRAW_BOX_DBLBORDER_L (1 << 4)
#define GFX_DRAW_BOX_DBLBORDER_R (1 << 5)
#define GFX_DRAW_BOX_DBLBORDER_H (GFX_DRAW_BOX_DBLBORDER_T | GFX_DRAW_BOX_DBLBORDER_B)
#define GFX_DRAW_BOX_DBLBORDER_V (GFX_DRAW_BOX_DBLBORDER_L | GFX_DRAW_BOX_DBLBORDER_R)
#define GFX_DRAW_BOX_DBLBORDER   (GFX_DRAW_BOX_DBLBORDER_H | GFX_DRAW_BOX_DBLBORDER_V)
void gfx_draw_box(int x, int y, int w, int h, uint8_t fillcolor, uint8_t bordercolor, unsigned flags);

#define GFX_DRAW_TEXT_CENTER     (1 << 0)
#define GFX_DRAW_TEXT_NOWRAP     (1 << 1)
#define GFX_DRAW_TEXT_SIMPLEWRAP (1 << 2)
#define GFX_DRAW_TEXT_NODOTS     (1 << 3)
void gfx_calc_text(int maxw, int maxh, char* text, int len, unsigned flags, int* w, int* h);
void gfx_draw_text(int x, int y, int w, int h, char* text, int len, uint8_t color, unsigned flags);

#endif
