#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#define VER_MAJOR 0
#define VER_MINOR 0
#define VER_PATCH 0

static char* filename = NULL;
static FILE* filedata = NULL;

enum __attribute__((packed)) cliopt_cs {
    CLIOPT_CS_ASCII,
    CLIOPT_CS_CP437,
    CLIOPT_CS_UTF8,
};
static struct {
    enum cliopt_cs cs;
    bool nocolor;
} cliopt = {0};

static void error(int e, const char* argv0, const char* f, ...) {
    fprintf(stderr, "%s: ", argv0);
    va_list v;
    va_start(v, f);
    vfprintf(stderr, f, v);
    va_end(v);
    exit(e);
}

static void puthelp(const char* argv0) {
    printf("Platinum Tracker version %d.%d.%d\n", VER_MAJOR, VER_MINOR, VER_PATCH);
    printf("\nUSAGE: %s [OPTION]... FILE\n", argv0);
    puts("\nOPTIONS:");
    puts("      --help        Display help text and exit");
    puts("      --version     Display version info and exit");
    puts("  -c, --charset     Character set to use (ascii, cp437, or utf-8)");
    puts("  -n, --no-color    Disable color");
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
                    if (i >= argc) error(1, argv[0], "Expected argument value for %s", optname);
                } else if ((shortopt) ? sopt == 'n' : !strcmp(opt, "no-color")) {
                    cliopt.nocolor = true;
                } else {
                    error(1, argv[0], "Unknown argument: %s", (shortopt) ? (char[]){'-', sopt, 0} : opt - 2);
                }
                if (shortopt) goto soret;
            } else {
                if (filename) error(1, argv[0], "File already provided %s");
                filename = strdup(argv[i]);
            }
        }
        if (filename) {
            filedata = fopen(filename, "rb+");
        } else {
            filename = strdup("untitled.ptm");
        }
    }
    return 0;
}
