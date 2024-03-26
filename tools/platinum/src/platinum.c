#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>

#include "string.h"
#include "time.h"

#define VER_MAJOR 0
#define VER_MINOR 0
#define VER_PATCH 0

static char* filename = NULL;
static FILE* filedata = NULL;

enum __attribute__((packed)) cliopt_c {
    CLIOPT_C_ASCII,
    CLIOPT_C_CP437,
    CLIOPT_C_UTF8,
    CLIOPT_C__COUNT
};
enum __attribute__((packed)) cliopt_o {
    CLIOPT_O_VT,
    CLIOPT_O_VTCOLOR,
    #ifdef _WIN32
    CLIOPT_O_CONHOST
    #endif
};
static struct {
    enum cliopt_c c;
    enum cliopt_o o;
} cliopt = {
    .c = CLIOPT_C_UTF8,
    .o = CLIOPT_O_VTCOLOR
};

static struct {
    unsigned w;
    unsigned h;
    struct termios t;
    struct termios nt;
    fd_set fds;
} tty;
static void cleanuptty(void) {
    write(STDOUT_FILENO, "\e[?25h\e[H\e[2J\e[3J\e[?1049l", 25);
    tcsetattr(STDIN_FILENO, TCSANOW, &tty.t);
}
static void updatetty(void) {
    #ifndef SIGWINCH
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    tty.w = w.ws_col;
    tty.h = w.ws_row;
    #endif
}
static void sigh(int sig) {
    (void)sig;
    cleanuptty();
    exit(0);
}
static void sigwinchh(int sig);
static void setuptty(void) {
    tcgetattr(STDIN_FILENO, &tty.t);
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
    #ifdef SIGWINCH
    signal(SIGWINCH, sigwinchh);
    #endif
    tty.nt = tty.t;
    tty.nt.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    tty.nt.c_cflag |= (CS8);
    tty.nt.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    //tty.nt.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &tty.nt);
    //fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    FD_ZERO(&tty.fds);
    write(STDOUT_FILENO, "\e[?1049h\e[?25l\e[2J\e[3J", 22);
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    tty.w = w.ws_col;
    tty.h = w.ws_row;
}

enum __attribute__((packed)) keytype {
    KT_NULL,
    KT_UNKOWN,
    KT_CHAR,
    KT_UP,
    KT_DOWN,
    KT_LEFT,
    KT_RIGHT,
    KT_HOME,
    KT_END,
    KT_PGUP,
    KT_PGDN,
    KT_FN,
    KF_SHIFT = (1 << 4),
    KF_CTRL = (1 << 5),
    KF_ALT = (1 << 6),
};
struct key {
    enum keytype t;
    union {
        char c;
        uint8_t fn;
    };
};
static struct {
    uint8_t r;
    uint8_t w;
    struct key d[256];
    struct charbuf cb;
} kbuf = {
    .r = 255
};
static bool getkey(uint64_t t, struct key* o) {
    if ((kbuf.r + 1) % 256 != kbuf.w) {
        ++kbuf.r;
        *o = kbuf.d[kbuf.r];
        return true;
    }
    int tmp;
    {
        struct timeval tv;
        tv.tv_sec = t / 1000000;
        tv.tv_usec = t % 1000000;
        FD_SET(STDIN_FILENO, &tty.fds);
        tmp = select(STDIN_FILENO + 1, &tty.fds, NULL, NULL, &tv);
        if (!tmp) return false;
        if (tmp < 0) {
            o->t = KT_NULL;
            return true;
        }
    }
    ioctl(STDIN_FILENO, FIONREAD, &tmp);
    bool wroteret = false;
    readkey:;
    char c = 0;
    read(STDIN_FILENO, &c, 1);
    --tmp;
    struct key k;
    if (c == '\e') {
        if (!tmp) {
            k.t = KT_CHAR;
            k.c = '\e';
        } else {
            k.t = KT_UNKOWN;
        }
    } else {
        k.t = KT_CHAR;
        k.c = c;
    }
    if (wroteret) {
        if (kbuf.r != kbuf.w) {
            kbuf.d[kbuf.w] = k;
            ++kbuf.w;
        }
    } else {
        *o = k;
        wroteret = true;
    }
    if (tmp) goto readkey;
    return true;
}

static struct {
    char* name;
    char* author;
    char* comments;
    float bpm;
} song = {
    .bpm = 150.0
};

enum __attribute__((packed)) drawcomponent {
    // U = up, D = down, L = left, R = right
    // V = vertical (up+down), H = horizontal (left+right)
    // B = bold/double
    DC_V, DC_VL, DC_VBL, DC_BVL, DC_BDL, DC_DBL, DC_BVBL, DC_BV, DC_BDBL, DC_BUBL,
    DC_BUL, DC_UBL, DC_DL, DC_UR, DC_UH, DC_DH, DC_VR, DC_H, DC_VH, DC_VBR,
    DC_BVR, DC_BUBR, DC_BDBR, DC_BUBH, DC_BDBH, DC_BVBR, DC_BH, DC_BVBH, DC_UBV, DC_BUV,
    DC_DBH, DC_BDH, DC_BUR, DC_UBR, DC_DBR, DC_BDR, DC_BVH, DC_VBH, DC_UL, DC_DR,
    DC__COUNT
};
static char* dctext[CLIOPT_C__COUNT][DC__COUNT] = {
    {
        "|", "|", "|", "|", ",", ".", "!", "!", ",", "\"",
        "\"", "'", ".", "'", "'", ".", "|", "-", "+", "|",
        "!", "\"", ",", "\"", ",", "!", "=", "#", "'", "\"",
        ".", ",", "\"", "'", ".", ",", "+", "+", "'", "."
    },
    {
        "\xB3", "\xB4", "\xB5", "\xB6", "\xB7", "\xB8", "\xB9", "\xBA", "\xBB", "\xBC",
        "\xBD", "\xBE", "\xBF", "\xC0", "\xC1", "\xC2", "\xC3", "\xC4", "\xC5", "\xC6",
        "\xC7", "\xC8", "\xC9", "\xCA", "\xCB", "\xCC", "\xCD", "\xCE", "\xCF", "\xD0",
        "\xD1", "\xD2", "\xD3", "\xD4", "\xD5", "\xD6", "\xD7", "\xD8", "\xD9", "\xDA"
    },
    {
        "│", "┤", "╡", "╢", "╖", "╕", "╣", "║", "╗", "╝",
        "╜", "╛", "┐", "└", "┴", "┬", "├", "─", "┼", "╞",
        "╟", "╚", "╔", "╩", "╦", "╠", "═", "╬", "╧", "╨",
        "╤", "╥", "╙", "╘", "╒", "╓", "╫", "╪", "┘", "┌"
    }
};
static char** curdctext;
#define PUTDC(x) fputs(curdctext[(x)], stdout)
static struct {
    struct charbuf title;
    int titleoff;
    union {
        unsigned drew;
        struct {
            unsigned drewframe : 1;
            unsigned drewtitle : 1;
        };
    };
} ui = {0};
static void sigwinchh(int sig) {
    (void)sig;
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    tty.w = w.ws_col;
    tty.h = w.ws_row;
    write(STDOUT_FILENO, "\e[H\e[2J\e[3J", 11);
    ui.drew = 0;
}
static void setuititle(const char* s) {
    cb_clear(&ui.title);
    cb_addstr(&ui.title, "Platinum Music Tracker ");
    char num[12];
    sprintf(num, "%d", VER_MAJOR);
    cb_addstr(&ui.title, num);
    cb_add(&ui.title, '.');
    sprintf(num, "%d", VER_MINOR);
    cb_addstr(&ui.title, num);
    cb_add(&ui.title, '.');
    sprintf(num, "%d", VER_PATCH);
    cb_addstr(&ui.title, num);
    cb_add(&ui.title, ' ');
    cb_add(&ui.title, '-');
    cb_add(&ui.title, ' ');
    cb_addstr(&ui.title, s);
    cb_addstr(&ui.title, "   ///   ");
    ui.titleoff = 0;
}
static void drawuiframe(void) {
    int sparew = tty.w - 2;
    int spareh = tty.h - 2;
    fputs("\e[H", stdout);
    PUTDC(DC_DBR);
    for (int i = 0; i < sparew; ++i) PUTDC(DC_BH);
    PUTDC(DC_DBL);
    for (int i = 0; i < spareh; ++i) {
        putchar('\n');
        PUTDC(DC_V);
        printf("\e[%d;%dH", i + 2, tty.w);
        PUTDC(DC_V);
    }
    putchar('\n');
    PUTDC(DC_UBR);
    for (int i = 0; i < sparew; ++i) PUTDC(DC_BH);
    PUTDC(DC_UBL);
}
static void drawuititle(void) {
    int tpos = (tty.w + 1) / 2 - ui.title.len / 2 - 2;
    if (tpos < 2) tpos = 2;
    int tmax = tty.w - 6;
    printf("\e[1;%dH", tpos);
    PUTDC(DC_VBL);
    putchar(' ');
    int i = ui.titleoff;
    int j = 1;
    while (1) {
        if (i < ui.title.len) putchar(ui.title.data[i]);
        else putchar(' ');
        i = (i + 1) % ui.title.len;
        if (i == ui.titleoff || j == ui.title.len ||  j == tmax) break;
        ++j;
    }
    putchar(' ');
    PUTDC(DC_VBR);
}
static void udvuititle(void) {
    ui.titleoff = (ui.titleoff + 1) % ui.title.len;
    ui.drewtitle = false;
}
static void drawui(void) {
    if (!ui.drewframe) {drawuiframe(); ui.drewframe = 1;}
    if (!ui.drewtitle) {drawuititle(); ui.drewtitle = 1;}
    fflush(stdout);
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
static int newtimer(uint64_t a, void* d) {
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
static void deltimer(int i) {
    tdata.data[i].a = 0;
}
static int getnexttimer(uint64_t* u) {
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

static void run(void) {
    enum __attribute__((packed)) ev {
        EV_NOTHING,
        EV_ADVTITLE
    };
    newtimer(200000, (void*)EV_ADVTITLE);
    while (1) {
        uint64_t tt;
        int ti = getnexttimer(&tt);
        recalctimer:;
        uint64_t d;
        if (ti >= 0) {
            uint64_t lt = altutime();
            if (tt > lt) d = tt - lt;
            else d = 0;
        } else {
            d = 0;
        }
        updatetty();
        drawui();
        struct key k;
        if (getkey(d, &k)) {
            printf("k.t: %d, k.c: %d\n", k.t, k.c);
            if (k.t == KT_CHAR && k.c == 'q') break;
            goto recalctimer;
        }
        if (ti >= 0) {
            switch ((enum ev)tdata.data[ti].d) {
                case EV_ADVTITLE: {
                    udvuititle();
                } break;
                default: break;
            };
        }
    }
}

static void die(const char* argv0, const char* f, ...) {
    fprintf(stderr, "%s: ", argv0);
    va_list v;
    va_start(v, f);
    vfprintf(stderr, f, v);
    va_end(v);
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
    puts("                        cp437: 8-bit code page 437");
    puts("                        utf-8: UTF-8 (default)");
    puts("  -o, --output      How to output color and move the cursor");
    puts("                        vt:      VT escape codes without color");
    puts("                        vtcolor: VT escape codes using color (default)");
    puts("                        conhost: Conhost functions (Windows only)");
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
                if (shortopt) {
                    if (!(sopt = opt[sopos])) continue;
                    optname = (char[]){'-', sopt, 0};
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
                         cliopt.c = CLIOPT_C_ASCII;
                    } else if (!strcmp(argv[i], "cp437") || !strcmp(argv[i], "437")) {
                         cliopt.c = CLIOPT_C_CP437;
                    } else if (!strcmp(argv[i], "utf-8") || !strcmp(argv[i], "utf8") || !strcmp(argv[i], "utf")) {
                         cliopt.c = CLIOPT_C_UTF8;
                    } else {
                        die(argv[0], "Invalid value for argument %s: %s", optname, argv[i]);
                    }
                } else if ((shortopt) ? sopt == 'o' : !strcmp(opt, "output")) {
                    ++i;
                    if (i >= argc) die(argv[0], "Expected argument value for %s", optname);
                    if (!strcmp(argv[i], "vt")) {
                         cliopt.o = CLIOPT_O_VT;
                    } else if (!strcmp(argv[i], "vtcolor")) {
                         cliopt.o = CLIOPT_O_VTCOLOR;
                    }
                    #ifdef _WIN32
                      else if (!strcmp(argv[i], "conhost")) {
                         cliopt.o = CLIOPT_O_CONHOST;
                    }
                    #endif
                      else {
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
    curdctext = dctext[cliopt.c];
    cb_init(&kbuf.cb, 16);
    cb_init(&ui.title, 256);
    setuititle(filename);
    tdata.len = 0;
    tdata.size = 4;
    tdata.data = malloc(tdata.size * sizeof(*tdata.data));
    setuptty();
    run();
    cleanuptty();
    return 0;
}
