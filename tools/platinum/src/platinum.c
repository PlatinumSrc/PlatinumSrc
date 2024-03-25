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
} ttyi;
static void cleanuptty(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &ttyi.t);
}
static void updatetty(void) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    ttyi.w = w.ws_col;
    ttyi.h = w.ws_row;
}
static void sigh(int sig) {
    (void)sig;
    cleanuptty();
    exit(0);
}
static void setuptty(void) {
    tcgetattr(STDIN_FILENO, &ttyi.t);
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
    ttyi.nt = ttyi.t;
    ttyi.nt.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    ttyi.nt.c_cflag |= (CS8);
    ttyi.nt.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    //ttyi.nt.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttyi.nt);
    //fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

enum __attribute__((packed)) keytype {
    KT_NULL,
    KT_CHAR,
    KT_UP,
    KT_DOWN,
    KT_LEFT,
    KT_RIGHT,
    KT_FN,
    KF_SHIFT = (1 << 4),
    KF_CTRL = (1 << 5),
    KF_ALT = (1 << 6),
};
struct key {
    enum keytype t;
    union {
        char ascii;
        uint8_t fn;
    };
};
static struct {
    uint8_t r;
    uint8_t w;
    struct key d[256];
} kbuf = {
    .r = 255
};
static struct key getkey(void) {
    int ct = 0;
    ioctl(0, FIONREAD, &ct);
    if (ct) {
        
    }
    if ((kbuf.r + 1) % 256 == kbuf.w) return (struct key){.t = KT_NULL};
    ++kbuf.r;
    return kbuf.d[kbuf.r];
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
        "|", "|", "|", "|", ".", ".", "|", "|", ".", "'",
        "'", "'", ".", "'", "'", ".", "|", "-", "+", "|",
        "|", "'", ".", "'", ".", "|", "-", "+", "'", "'",
        ".", ".", "'", "'", ".", ".", "+", "+", "'", "."
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
static void draw(void) {
    updatetty();
    
}

static void run(void) {

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
    setuptty();
    run();
    cleanuptty();
    return 0;
}
