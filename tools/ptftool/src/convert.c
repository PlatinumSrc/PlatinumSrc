#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>

#include <lz4/lz4file.h>
#include <lz4/lz4hc.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize.h>

#include <psrc/engine/ptf.h>

static struct {
    bool overwrite;
    int8_t alpha;
} opt = {
    .alpha = -1
};

static void img2ptf(char* p) {
    char* np;
    {
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
                np[++i] = 'p';
                np[++i] = 't';
                np[++i] = 'f';
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
        np[++i] = 'p';
        np[++i] = 't';
        np[++i] = 'f';
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
    }
    FILE* fin = fopen(p, "rb");
    if (!fin) {
        fputs(" failed (could not open input: ", stdout);
        fputs(strerror(errno), stdout);
        fputs(")\n", stdout);
        free(np);
        return;
    }
    FILE* fout;
    if (!opt.overwrite) {
        if ((fout = fopen(np, "rb"))) {
            fputs(" failed (output exists)\n", stdout);
            fclose(fout);
            fclose(fin);
            free(np);
            return;
        }
    }
    int w, h, c, nw, nh, pw, ph;
    void* data = stbi_load_from_file(fin, &w, &h, &c, (opt.alpha == -1) ? 0 : (3 + opt.alpha));
    if (!data) {
        fputs(" failed (stb_image failed to decode input)\n", stdout);
        fclose(fin);
        free(np);
        return;
    }
    if (opt.alpha == -1) {
        if (c < 3) {
            printf(" failed (invalid channel count: %d)\n", c);
            free(data);
            fclose(fin);
            free(np);
            return;
        }
    } else {
        c = (3 + opt.alpha);
    }
    nw = w - 1;
    nh = h - 1;
    nw |= nw >> 1;
    nw |= nw >> 2;
    nw |= nw >> 4;
    nw |= nw >> 8;
    nw |= nw >> 16;
    nh |= nh >> 1;
    nh |= nh >> 2;
    nh |= nh >> 4;
    nh |= nh >> 8;
    nh |= nh >> 16;
    if ((nw | nh) & 0xFFFF8000) {
        fputs(" failed (image too large)\n", stdout);
        free(data);
        fclose(fin);
        free(np);
        return;
    }
    ++nw;
    ++nh;
    {
        int tmp = nw >> 1;
        pw = 0;
        while (tmp) {
            ++pw;
            tmp >>= 1;
        }
        tmp = nh >> 1;
        ph = 0;
        while (tmp) {
            ++ph;
            tmp >>= 1;
        }
    }
    if (w != nw || h != nh) {
        void* tmp = malloc(nw * nh * c);
        stbir_resize_uint8(data, w, h, 0, tmp, nw, nh, 0, c);
        free(data);
        data = tmp;
    }
    fout = fopen(np, "wb");
    if (!fout) {
        fputs(" failed (could not open output: ", stdout);
        fputs(strerror(errno), stdout);
        fputs(")\n", stdout);
        fclose(fin);
        free(np);
        return;
    }
    fputc('P', fout);
    fputc('T', fout);
    fputc('F', fout);
    fputc(PTF_REV, fout);
    fputc(c - 3, fout);
    fputc((ph << 4) | pw, fout);
    LZ4_writeFile_t* wf;
    LZ4F_preferences_t lzp = LZ4F_INIT_PREFERENCES;
    lzp.compressionLevel = LZ4HC_CLEVEL_MAX;
    if (LZ4F_isError(LZ4F_writeOpen(&wf, fout, &lzp))) {
        fputs(" failed (could not create LZ4 context)\n", stdout);
        free(data);
        fclose(fout);
        fclose(fin);
        free(np);
        return;
    }
    LZ4F_errorCode_t e = LZ4F_write(wf, data, nw * nh * c);
    if (LZ4F_isError(e)) {
        printf(" failed (LZ4 error %ld: %s)\n", (long)e, LZ4F_getErrorName(e));
        LZ4F_writeClose(wf);
        free(data);
        fclose(fout);
        fclose(fin);
        free(np);
        return;
    }
    LZ4F_writeClose(wf);
    free(data);
    fclose(fout);
    fclose(fin);
    free(np);
    
    fputs(" done\n", stdout);
}

int ptf_convert(char* argv0, int argc, char** argv) {
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
            } else if ((shortopt) ? sopt == 'a' : !strcmp(lopt, "force-alpha")) {
                opt.alpha = 1;
            } else if ((shortopt) ? sopt == 'A' : !strcmp(lopt, "remove-alpha")) {
                opt.alpha = 0;
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
            img2ptf(argv[i]);
        }
    }
    return 0;
}
