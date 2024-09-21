#ifndef PLATINUM_GFX_H
#define PLATINUM_GFX_H

#include <stdint.h>
#include <stdbool.h>

enum __attribute__((packed)) gfx_charset {
    GFX_CHARSET_ASCII,
    GFX_CHARSET_CP437,
    GFX_CHARSET_UTF8,
    GFX_CHARSET__COUNT
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
#define GFX_EXTCOLOR_CUBE(r, g, b)  (16 + (r) * 36 + (g) * 6 + (b))
#define GFX_EXTCOLOR_CUBE8(r, g, b) GFX_EXTCOLOR_CUBE((r) * 6 / 256, + (g) * 6 / 256, (b) * 6 / 256)
#define GFX_EXTCOLOR_RAMP(w)        (232 + (w))
#define GFX_EXTCOLOR_RAMP8(w)       GFX_EXTCOLOR_RAMP((w) * 24 / 256)

struct __attribute__((packed)) gfx_char {
    uint32_t fgc : 8;
    uint32_t bgc : 8;
    uint32_t mouse : 1;
    uint32_t : 7;
    uint32_t chr : 8;
};
struct __attribute__((packed)) gfx_linechange {
    int16_t l, r;
};
extern struct gfx {
    struct {
        struct gfx_char* data;
        int w, h;
        int neww, newh;
        struct gfx_linechange* linechange;
    } screen;
    bool redraw : 1;
    bool resize : 1;
    enum gfx_charset charset;
    struct {
        int16_t x, y;
    } mouse, oldmouse;
} gfx;

void gfx_setup(void);
void gfx_cleanup(void);

static inline bool gfx_getlineredraw(int y) {
    return (gfx.screen.linechange[y].l >= 0);
}
bool gfx_getlinesredraw(int y1, int y2);
bool gfx_getregionredraw(int x1, int y1, int x2, int y2);

void gfx_setlineredraw(int y);
void gfx_setlinesredraw(int y1, int y2);
void gfx_setregionredraw(int x1, int y1, int x2, int y2);

void gfx_update(void (*draw)(void));

#define GFX_DRAW_BAR_CENTERTEXT (1 << 0)

void gfx_draw_bar(unsigned y, uint8_t fgc, uint8_t bgc, char* text, int len, unsigned ml, unsigned mr, unsigned flags);

#endif
