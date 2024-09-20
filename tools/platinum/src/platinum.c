#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <signal.h>
#include <inttypes.h>
#include <ctype.h>
#include <wchar.h>
#ifndef _WIN32
    #include <termios.h>
    #include <sys/ioctl.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

#include "string.h"
#include "time.h"

#define VER_MAJOR 0
#define VER_MINOR 0
#define VER_PATCH 0

static char* filename = NULL;
static FILE* filedata = NULL;

static struct {
    #ifndef _WIN32
    struct termios t;
    struct termios nt;
    fd_set fds;
    #endif
} tty;
static void tty_setup(void) {
    #ifndef _WIN32
    tcgetattr(STDIN_FILENO, &tty.t);
    tty.nt = tty.t;
    tty.nt.c_iflag &= ~(BRKINT | IGNCR | INPCK | ISTRIP | IXON);
    tty.nt.c_cflag |= (CS8);
    tty.nt.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    //tty.nt.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &tty.nt);
    //fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    FD_ZERO(&tty.fds);
    static char* s = "\e[?1049h\e[H\e[?25l\e[2J\e[3J\e[?1003h\e[?1006h";
    write(STDOUT_FILENO, s, strlen(s));
    #endif
}
static void tty_cleanup(void) {
    #ifndef _WIN32
    static char* s = "\e[?1006l\e[?1003l\e[H\e[?25h\e[2J\e[3J\e[?1049l";
    write(STDOUT_FILENO, s, strlen(s));
    tcsetattr(STDIN_FILENO, TCSANOW, &tty.t);
    #endif
}

enum __attribute__((packed)) keytype {
    KT_UNKNOWN,
    KT_CHAR,
    KT_SP,
    KT_NAV,
    KT_FN,
    KT_MOUSEMOVE,
    KT_MOUSEBUTTON,
    KT_MOUSESCROLL,
    KF_SHIFT = (1 << 5),
    KF_CTRL = (1 << 6),
    KF_ALT = (1 << 7),
};
enum __attribute__((packed)) key_sp {
    K_SP_BACKSPACE,
    K_SP_ENTER,
    K_SP_ESC,
    K_SP_DELETE
};
enum __attribute__((packed)) key_nav {
    K_NAV_UP,
    K_NAV_DOWN,
    K_NAV_LEFT,
    K_NAV_RIGHT,
    K_NAV_HOME,
    K_NAV_END,
    K_NAV_PGUP,
    K_NAV_PGDN
};
#define KT_MASK 0x1F
#define KF_MASK 0xE0
struct key {
    enum keytype t;
    union {
        uint32_t c;
        enum key_sp sp;
        enum key_nav nav;
        uint8_t fn;
        struct {
            uint16_t mx;
            uint16_t my;
        };
        struct {
            uint8_t mbdn : 1;
            uint8_t mb : 7;
        };
        int8_t ms;
    };
};
#define KBUF_RINGSIZE 256
static struct {
    struct key ring[KBUF_RINGSIZE];
    int ringstart;
    int ringnext;
    int ringsize;
    char* inbuf;
    int inbufsz;
} kbuf;
static bool kbuf_update(void) {
    int len;
    if (ioctl(STDIN_FILENO, FIONREAD, &len) < 0) return false;
    if (kbuf.inbufsz < len || kbuf.inbufsz - len > 4096) {
        kbuf.inbufsz = len;
        kbuf.inbuf = realloc(kbuf.inbuf, len);
    }
    read(STDIN_FILENO, kbuf.inbuf, len);
    char* s = kbuf.inbuf;
    #define kbu_add(...) do {\
        struct key k = {.t = __VA_ARGS__};\
        if ((k.t & KT_MASK) == KT_CHAR && (k.t & KF_MASK) && isupper(k.c)) {\
            k.t |= KF_SHIFT;\
            k.c = tolower(k.c);\
        }\
        kbuf.ring[kbuf.ringnext] = k;\
        kbuf.ringnext = (kbuf.ringnext + 1) % KBUF_RINGSIZE;\
        ++kbuf.ringsize;\
    } while (0)
    while (1) {
        char c = *s++;
        --len;
        switch (c) {
            default: kbu_add(KT_CHAR, .c = c); break;
            case '\x08': kbu_add(KT_SP | KF_CTRL, .sp = K_SP_BACKSPACE); break;
            case '\n': kbu_add(KT_SP, .sp = K_SP_ENTER); break;
            case '\e':
                if (!len) {
                    kbu_add(KT_CHAR, .c = '\e');
                    break;
                }
                char* olds = s;
                c = *s++;
                int oldlen = len--;
                switch (c) {
                    default: kbu_add(KT_CHAR | KF_ALT, .c = c); goto escok;
                    case '\x08': kbu_add(KT_SP | KF_CTRL | KF_ALT, .sp = K_SP_BACKSPACE); goto escok;
                    case '\n': kbu_add(KT_SP | KF_ALT, .sp = K_SP_ENTER); goto escok;
                    case '[': {
                        if (!len) break;
                        c = *s++;
                        --len;
                        switch (c) {
                            case 'A': kbu_add(KT_NAV, .nav = K_NAV_UP); goto escok;
                            case 'B': kbu_add(KT_NAV, .nav = K_NAV_DOWN); goto escok;
                            case 'C': kbu_add(KT_NAV, .nav = K_NAV_RIGHT); goto escok;
                            case 'D': kbu_add(KT_NAV, .nav = K_NAV_LEFT); goto escok;
                            case '3':
                                if (!len) break;
                                c = *s++;
                                --len;
                                if (c == '~') {
                                    kbu_add(KT_SP, .sp = K_SP_DELETE);
                                    goto escok;
                                } else if (len >= 2 && c == ';' && s[1] == '~') {
                                    enum keytype t = KT_SP;
                                    c = *s - '1';
                                    if (c & 1) t |= KF_SHIFT;
                                    if (c & 2) t |= KF_ALT;
                                    if (c & 4) t |= KF_CTRL;
                                    kbu_add(t, .sp = K_SP_DELETE);
                                    s += 2;
                                    len -= 2;
                                    goto escok;
                                }
                                break;
                            case '<': {
                                if (len < 6) break;
                                int i = 5;
                                while (1) {
                                    char c = s[i++];
                                    if (c == 'M' || c == 'm') {
                                        uint8_t b;
                                        unsigned x, y;
                                        enum keytype to = 0;
                                        sscanf(s, "%" SCNu8 ";%u;%uM", &b, &x, &y);
                                        if (b & 4) to |= KF_SHIFT;
                                        if (b & 8) to |= KF_ALT;
                                        if (b & 16) to |= KF_CTRL;
                                        if (!(c & 32)) {
                                            if (!(b & 32)) {
                                                if (!(b & 64)) kbu_add(KT_MOUSEBUTTON | to, .mbdn = 1, .mb = b & 3);
                                                else kbu_add(KT_MOUSESCROLL | to, .ms = (b & 1) ? 1 : -1);
                                            }
                                        } else {
                                            kbu_add(KT_MOUSEBUTTON | to, .mb = b & 3);
                                        }
                                            kbu_add(KT_MOUSEMOVE | to, .mx = x - 1, .my = y - 1);
                                        s += i;
                                        len -= i;
                                        goto escok;
                                    }
                                    if (i == len) break;
                                }
                            } break;
                        }
                    } break;
                    case '\e':
                        if (len >= 2 && *s == 'O' && s[1] == 'M') {
                            kbu_add(KT_SP | KF_SHIFT | KF_ALT, .sp = K_SP_ENTER);
                            s += 2;
                            len -= 2;
                            goto escok;
                        }
                        kbu_add(KT_SP | KF_ALT, .sp = K_SP_ESC);
                        goto escok;
                    case 'O':
                        if (len) {
                            c = *s++;
                            --len;
                            if (c == 'M') {
                                kbu_add(KT_SP | KF_SHIFT, .sp = K_SP_ENTER);
                                goto escok;
                            } else if (c >= 'P' && c <= 'S') {
                                kbu_add(KT_FN, .fn = c - 'P' + 1);
                                goto escok;
                            }
                        }
                        kbu_add(KT_CHAR | KF_SHIFT | KF_ALT, .c = 'o');
                        goto escok;
                    case '\x7F': kbu_add(KT_SP | KF_ALT, .sp = K_SP_BACKSPACE); goto escok;
                }
                s = olds;
                len = oldlen;
                kbu_add(KT_UNKNOWN);
                escok:;
                break;
            case '\x00' ... '\x07':
            case '\x09':
            case '\x0B' ... '\x1A':
            case '\x1C' ... '\x1F':
                kbu_add(KT_CHAR | KF_CTRL, .c = tolower(c + '@'));
                break;
            case '\x7F': kbu_add(KT_SP, .sp = K_SP_BACKSPACE); break;
        }
        if (!len) break;
    }
    #undef kbu_add
    return true;
}
static bool getkey(struct key* o) {
    if (!kbuf.ringsize) return false;
    *o = kbuf.ring[kbuf.ringstart];
    kbuf.ringstart = (kbuf.ringstart + 1) % KBUF_RINGSIZE;
    --kbuf.ringsize;
    return true;
}

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
#define GFX_EXTCOLOR_CUBE(r, g, b) (16 + (r) * 36 + (g) * 6 + (b))
#define GFX_EXTCOLOR_CUBE8(r, g, b) GFX_EXTCOLOR_CUBE((r) * 6 / 256, + (g) * 6 / 256, (b) * 6 / 256)
#define GFX_EXTCOLOR_RAMP(w) (232 + (w))
#define GFX_EXTCOLOR_RAMP8(w) GFX_EXTCOLOR_RAMP((w) * 24 / 256)
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
static struct {
    struct {
        struct gfx_char* data;
        int w, h;
        int neww, newh;
        struct gfx_linechange* linechange;
        #ifdef _WIN32
        struct termios oldtio, newtio;
        #endif
    } screen;
    bool redraw : 1;
    bool resize : 1;
    enum gfx_charset charset;
    struct {
        int16_t x, y;
    } mouse, oldmouse;
    #ifdef _WIN32
    fd_set fds;
    #endif
} gfx = {
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
static void gfx_setup(void) {
    gfx_updatescreensize();
    #ifndef _WIN32
    signal(SIGWINCH, gfx_updatescreensize);
    #endif
}
static void gfx_cleanup(void) {
    #ifndef _WIN32
    static char* s = "\e[0m";
    write(STDOUT_FILENO, s, strlen(s));
    #endif
}
static inline bool gfx_getlineredraw(int y) {
    return (gfx.screen.linechange[y].l >= 0);
}
static bool gfx_getlinesredraw(int y1, int y2) {
    for (int y = y1; y <= y2; ++y) {
        if (gfx.screen.linechange[y].l >= 0) return true;
    }
    return false;
}
static bool gfx_getregionredraw(int x1, int y1, int x2, int y2) {
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
static void gfx_setlineredraw(int y) {
    gfx_screen__setchangedrange(y, 0, gfx.screen.w - 1);
}
static void gfx_setlinesredraw(int y1, int y2) {
    for (int y = y1; y <= y2; ++y) {
        gfx_screen__setchangedrange(y, 0, gfx.screen.w - 1);
    }
}
static void gfx_setregionredraw(int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2; ++y) {
        gfx_screen__setchangedrange(y, x1, x2);
    }
}
static void gfx_update(void (*draw)(void)) {
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
#define GFX_DRAW_BAR_CENTERTEXT (1 << 0)
static void gfx_draw_bar(unsigned y, uint8_t fgc, uint8_t bgc, char* text, int len, unsigned ml, unsigned mr, unsigned flags) {
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

struct timer {
    uint64_t t;
    uint64_t a;
    void* d;
};
static struct {
    struct timer* data;
    int len;
    int size;
    int index;
    int next;
} tdata = {
    .next = -1
};
static void timer_init(void) {
    tdata.len = 0;
    tdata.size = 4;
    tdata.data = malloc(tdata.size * sizeof(*tdata.data));
}
static int timer_new(uint64_t a, void* d) {
    int i = 0;
    for (; i < tdata.len; ++i) {
        if (!tdata.data[i].a) goto found;
    }
    if (i == tdata.size) {
        tdata.size *= 2;
        tdata.data = realloc(tdata.data, tdata.size * sizeof(*tdata.data));
    }
    ++tdata.len;
    found:;
    tdata.data[i].t = altutime() + a;
    tdata.data[i].a = a;
    tdata.data[i].d = d;
    return i;
}
static void timer_delete(int i) {
    tdata.data[i].a = 0;
}
static int timer_getnext(uint64_t* u) {
    if (!tdata.len) return -1;
    uint64_t m;
    int cur = tdata.next;
    int next = -1;
    if (cur == -1) {
        cur = 0;
        m = tdata.data[0].t;
        for (int i = 1; i < tdata.len; ++i) {
            if (tdata.data[i].t < m) {
                cur = i;
                m = tdata.data[i].t;
            }
        }
    }
    *u = tdata.data[cur].t;
    tdata.data[cur].t += tdata.data[cur].a;
    for (int i = 0; i < tdata.len; ++i) {
        if (next == -1 || tdata.data[i].t < m) {
            next = i;
            m = tdata.data[i].t;
        }
    }
    tdata.next = next;
    return cur;
}

static struct {
    char* name;
    char* author;
    char* comments;
    float bpm;
} song = {
    .bpm = 150.0
};

uint8_t titlecolors[] = {
    GFX_EXTCOLOR_CUBE(0, 0, 0),
    GFX_EXTCOLOR_CUBE(0, 0, 0),
    GFX_EXTCOLOR_CUBE(0, 0, 0),
    GFX_EXTCOLOR_CUBE(0, 0, 1),
    GFX_EXTCOLOR_CUBE(0, 0, 2),
    GFX_EXTCOLOR_CUBE(0, 0, 3),
    GFX_EXTCOLOR_CUBE(0, 0, 4),
    GFX_EXTCOLOR_CUBE(0, 0, 5),
    GFX_EXTCOLOR_CUBE(1, 1, 5),
    GFX_EXTCOLOR_CUBE(2, 2, 5),
    GFX_EXTCOLOR_CUBE(3, 3, 5),
    GFX_EXTCOLOR_CUBE(4, 4, 5),
    GFX_EXTCOLOR_CUBE(5, 5, 5),
    GFX_EXTCOLOR_CUBE(5, 5, 5),
    GFX_EXTCOLOR_CUBE(5, 5, 5),
    GFX_EXTCOLOR_CUBE(4, 4, 5),
    GFX_EXTCOLOR_CUBE(3, 3, 5),
    GFX_EXTCOLOR_CUBE(2, 2, 5),
    GFX_EXTCOLOR_CUBE(1, 1, 5),
    GFX_EXTCOLOR_CUBE(0, 0, 5),
    GFX_EXTCOLOR_CUBE(0, 0, 4),
    GFX_EXTCOLOR_CUBE(0, 0, 3),
    GFX_EXTCOLOR_CUBE(0, 0, 2),
    GFX_EXTCOLOR_CUBE(0, 0, 1),
};
int titlecolor = 0;
#define TITLECOLORS_COUNT (sizeof(titlecolors) / sizeof(*titlecolors))
static void draw(void) {
    if (gfx.screen.w < 40 || gfx.screen.h < 20) {
        //gfx_draw_text(0, 0, GFX_COLOR_WHITE, GFX_COLOR_RED, "TERMINAL TOO SMALL");
        return;
    }
    if (gfx_getlineredraw(0)) {
        gfx_draw_bar(0, titlecolors[titlecolor], GFX_COLOR_WHITE, "Platinum Music Tracker", -1, 1, 1, GFX_DRAW_BAR_CENTERTEXT);
    }
    if (gfx_getlineredraw(1)) {
        gfx_draw_bar(1, GFX_COLOR_BLACK, GFX_COLOR_WHITE, " File  Edit  Song  Page  Track  Help ", -1, 1, 1, 0);
    }
    if (gfx_getlineredraw(gfx.screen.h - 1)) {
        gfx_draw_bar(gfx.screen.h - 1, GFX_COLOR_BLACK, GFX_COLOR_WHITE, "Idle", -1, 1, 1, 0);
    }
}
static void run(void) {
    enum __attribute__((packed)) ev {
        EV_NOTHING,
        EV_TITLE
    };
    timer_new(250000, (void*)EV_TITLE);
    while (1) {
        uint64_t tt;
        int ti = timer_getnext(&tt);
        recalctimer:;
        int selret;
        uint64_t d;
        FD_SET(STDIN_FILENO, &tty.fds);
        if (ti >= 0) {
            uint64_t lt = altutime();
            if (tt > lt) d = tt - lt;
            else d = 0;
            struct timeval tv;
            tv.tv_sec = d / 1000000;
            tv.tv_usec = d % 1000000;
            selret = select(STDIN_FILENO + 1, &tty.fds, NULL, NULL, &tv);
        } else {
            selret = select(STDIN_FILENO + 1, &tty.fds, NULL, NULL, NULL);
        }
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
            switch ((enum ev)tdata.data[ti].d) {
                default: break;
                case EV_TITLE:
                    titlecolor = (titlecolor + 1) % TITLECOLORS_COUNT;
                    gfx_setlineredraw(0);
                    gfx.redraw = true;
                    break;
            };
        }
        gfx_update(draw);
        if (selret) goto recalctimer;
    }
}

static inline void setup() {
    tty_setup();
    gfx_setup();
    timer_init();
    gfx_update(draw);
}
static inline void cleanup() {
    gfx_cleanup();
    tty_cleanup();
}
static void sigh(int sig) {
    (void)sig;
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
    signal(SIGINT, sigh);
    signal(SIGTERM, sigh);
    #ifdef SIGQUIT
    signal(SIGQUIT, sigh);
    #endif
    #ifdef SIGUSR1
    signal(SIGUSR1, SIG_IGN);
    #endif
    #ifdef SIGUSR2
    signal(SIGUSR2, SIG_IGN);
    #endif
    #ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    #endif
    setup();
    run();
    cleanup();
    return 0;
}
