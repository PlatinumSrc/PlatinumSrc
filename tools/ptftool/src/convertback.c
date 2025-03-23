#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <psrc/engine/ptf.h>

static struct {
    bool overwrite;
    enum {
        FMT_PNG,
        FMT_BMP,
        FMT_TGA,
        FMT_JPG,
    } format;
    int quality;
} opt;

static void ptf2img(char* p) {
    char* np;
    {
        char fext[3];
        switch (opt.format) {
            default /*case FMT_PNG*/:
                fext[0] = 'p';
                fext[1] = 'n';
                fext[2] = 'g';
                break;
            case FMT_BMP:
                fext[0] = 'b';
                fext[1] = 'm';
                fext[2] = 'p';
                break;
            case FMT_TGA:
                fext[0] = 't';
                fext[1] = 'g';
                fext[2] = 'a';
                break;
            case FMT_JPG:
                fext[0] = 'j';
                fext[1] = 'p';
                fext[2] = 'g';
                break;
        }
        int l;
        l = strlen(p) + 1;
        np = malloc(l);
        memcpy(np, p, l);
        --l;
        int i = l - 1;
        while (i >= 0) {
            if (np[i] == '.') {
                if (i != l - 4) {
                    np = realloc(np, i + 5);
                    np[i + 4] = 0;
                }
                np[++i] = fext[0];
                np[++i] = fext[1];
                np[++i] = fext[2];
                goto done;
            } else if (np[i] == '/') {
                break;
            #ifdef _WIN32
            } else if (np[i] == '\\') {
                break;
            #endif
            }
            --i;
        }
        np = realloc(np, l + 5);
        i = l;
        np[i] = '.';
        np[++i] = fext[0];
        np[++i] = fext[1];
        np[++i] = fext[2];
        np[++i] = 0;
    }
    done:;
    fputs("Converting '", stdout);
    fputs(p, stdout);
    fputs("' to '", stdout);
    fputs(np, stdout);
    fputs("'...", stdout);
    fflush(stdout);

    {
        DIR* d = opendir(p);
        if (d) {
            fputs(" failed (input is a directory)\n", stdout);
            free(np);
            return;
        }
        if (!opt.overwrite) {
            FILE* f = fopen(np, "rb");
            if (f) {
                fputs(" failed (output exists)\n", stdout);
                fclose(f);
                free(np);
                return;
            }
        }
    }
    unsigned w, h, c;
    void* data;
    {
        FILE* f = fopen(p, "rb");
        if (!f) {
            fputs(" failed (could not open input: ", stdout);
            fputs(strerror(errno), stdout);
            fputs(")\n", stdout);
            free(np);
            return;
        }
        data = ptf_load(f, &w, &h, &c);
        fclose(f);
        if (!data) {
            fputs(" failed (could not decode PTF)\n", stdout);
            free(np);
            return;
        }
    }
    switch (opt.format) {
        case FMT_PNG:
            if (opt.quality) stbi_write_png_compression_level = opt.quality;
            if (stbi_write_png(np, w, h, c, data, 0)) goto noerr;
            break;
        case FMT_BMP:
            if (stbi_write_bmp(np, w, h, c, data)) goto noerr;
            break;
        case FMT_TGA:
            if (stbi_write_tga(np, w, h, c, data)) goto noerr;
            break;
        case FMT_JPG:
            if (stbi_write_jpg(np, w, h, c, data, (opt.quality) ? opt.quality : 90)) goto noerr;
            break;
    }
    fputs(" failed (stb_image_write failed)\n", stdout);
    free(data);
    free(np);
    return;
    noerr:;
    free(data);
    free(np);
    
    fputs(" done\n", stdout);
}

int ptf_convertback(char* argv0, int argc, char** argv) {
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
            if ((shortopt) ? sopt == 'o' : !strcmp(lopt, "overwrite")) {
                opt.overwrite = true;
            } else if ((shortopt) ? sopt == 'f' : !strcmp(lopt, "format")) {
                ++i;
                if (i >= argc) {
                    fputs(argv0, stderr);
                    fputs(": Expected argument\n", stderr);
                    return 1;
                }
                if (!strcmp(argv[i], "png")) {
                    opt.format = FMT_PNG;
                } else if (!strcmp(argv[i], "bmp")) {
                    opt.format = FMT_BMP;
                } else if (!strcmp(argv[i], "tga")) {
                    opt.format = FMT_TGA;
                } else if (!strcmp(argv[i], "jpg")) {
                    opt.format = FMT_JPG;
                } else {
                    fputs(argv0, stderr);
                    fputs(": Unknown format '", stderr);
                    fputs(argv[i], stderr);
                    fputs("'\n", stderr);
                    return 1;
                }
            } else if ((shortopt) ? sopt == 'q' : !strcmp(lopt, "quality")) {
                ++i;
                if (i >= argc) {
                    fputs(argv0, stderr);
                    fputs(": Expected argument\n", stderr);
                    return 1;
                }
                opt.quality = atoi(argv[i]);
            } else {
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
            ptf2img(argv[i]);
        }
    }
    return 0;
}
