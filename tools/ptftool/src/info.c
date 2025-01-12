#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>

#include <psrc/engine/ptf.h>

static void ptfinf(char* p) {
    fputs(p, stdout);
    putchar(':');
    {
        DIR* d = opendir(p);
        if (d) {
            fputs(" failed (is a directory)\n", stdout);
            return;
        }
    }
    FILE* f = fopen(p, "rb");
    if (!f) {
        fputs(" failed (could not open: ", stdout);
        fputs(strerror(errno), stdout);
        fputs(")\n", stdout);
        return;
    }
    int c;
    if (fgetc(f) != 'P' || fgetc(f) != 'T' || fgetc(f) != 'F' || (c = fgetc(f)) == EOF) {
        fputs(" failed (not a PTF file)\n", stdout);
        return;
    }
    if (c != PTF_REV) {
        fprintf(stderr, " failed (incorrect revision %d (expected %d))\n", c, PTF_REV);
    }
    if ((c = fgetc(f)) == EOF) {
        fputs(" failed (not a PTF file)\n", stdout);
        return;
    }
    putchar('\n');
    int r = c & 0xF;
    r = 1 << r;
    printf("    Resolution: %dx%d\n", r, r);
    fputs("    Format: RGB", stdout);
    if (c & 0x10) putchar('A');
    putchar('\n');
}

int ptf_info(char* argv0, int argc, char** argv) {
    bool onlyfile = false;
    for (int i = 0; i < argc; ++i) {
        if (!onlyfile && argv[i][0] == '-' && argv[i][1]) {
            bool shortopt = !(argv[i][1] == '-');
            if (!shortopt && !argv[i][2]) {onlyfile = true; continue;}
            char* lopt = argv[i];
            char sopt = 0;
            int sopos = 0;
            soret:;
            ++sopos;
            if (shortopt) {
                if (!(sopt = lopt[sopos])) continue;
            } else {
                lopt += 2;
            }
            {
                fputs(argv0, stderr);
                fputs(": Unknown option '", stderr);
                if (shortopt) {
                    char tmp[3] = {'-', sopt, 0};
                    fputs(tmp, stderr);
                } else {
                    fputs(argv[i], stderr);
                }
                fputs("'\n", stderr);
                return 1;
            }
            if (shortopt) goto soret;
        } else {
            ptfinf(argv[i]);
        }
    }
    return 0;
}
