#include "common.h"

#include <psrc/common/paf.h>
#include <psrc/byteorder.h>
#include <psrc/vlb.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

const char* paftool_create_help =
   "OUTPUT [OPTION]... [--] [PATH]... - Create an archive out of the provided PATHs\n"
   "        -c, --compress WHAT     - Compress the given type of files (can be used multiple times and in conjunction with -C)\n"
   "            WHAT    - a, all            - compress all files\n"
   "                      b, bin, binary    - compress non-empty files that contain non-printable characters\n"
   "                      t, text           - compress non-empty files that do not contain non-printable characters\n"
   "                      .EXT              - compress all files whose file extension matches EXT\n"
   "        -C, --no-compress WHAT  - Do not compress the given type of files (takes the same values as -c for WHAT)\n"
   "        -f, --force             - Overwrite\n"
   "        -l, --resolve-links     - Detect and attempt to turn symlinks into links in the archive\n"
   "        -L, --ignore-links      - Detect and ignore symlinks\n"
   "        -v, --verbose           - For each path added, give details about it after that addition\n"
   "        -w, --writing           - Optimize for writing instead of size (default if no PATHs are provided)"
;

struct ftreeuserdata VLB(uint32_t);

static void paftool_create_wrstr(FILE* f, const char* s, uint32_t* o) {
    uint32_t l = strlen(s);
    uint32_t tmpu32 = swaple32(l);
    fwrite(&tmpu32, 4, 1, f);
    fwrite(&tmpu32, 4, 1, f);
    fwrite(s, 1, l, f);
    *o += 8 + l;
}
static void paftool_create_wrdirs(FILE* f, struct ftree* tree, struct ftreeuserdata* userdata, uint32_t* doff) {
    uint32_t tmpu32 = swaple32(tree->len);
    fwrite(&tmpu32, 4, 1, f);
    fwrite(&tmpu32, 4, 1, f);
    fwrite(&tmpu32, 4, 1, f);
    tmpu32 = 0;
    fwrite(&tmpu32, 4, 1, f);
    *doff += 16;
    uint32_t tmpdoff = *doff;
    for (size_t i = 0; i < tree->len; ++i) {
        fputc(0, f);
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
    }
    if (!tree->len) return;
    *doff += 13 * tree->len;
    fseek(f, tmpdoff, SEEK_SET);
    {
        uint32_t tmpdoff2 = tmpdoff;
        for (size_t i = 0; i < tree->len; ++i) {
            struct ftree_node* n = &tree->data[i];
            fputc(!n->isdir, f);
            tmpu32 = swaple32(*doff);
            fwrite(&tmpu32, 4, 1, f);
            tmpu32 = swaple32(n->namecrc);
            fwrite(&tmpu32, 4, 1, f);
            tmpu32 = 0;
            fwrite(&tmpu32, 4, 1, f);
            tmpdoff2 += 13;
            fseek(f, *doff, SEEK_SET);
            paftool_create_wrstr(f, n->name, doff);
            fseek(f, tmpdoff2, SEEK_SET);
        }
    }
    tmpdoff += 9;
    fseek(f, tmpdoff, SEEK_SET);
    for (size_t i = 0; i < tree->len; ++i) {
        struct ftree_node* n = &tree->data[i];
        if (n->isdir) {
            n->userdata = (void*)userdata->len;
            VLB_ADD(*userdata, *doff, 2, 1, VLB_OOM_NOP);
            tmpu32 = swaple32(*doff);
            fwrite(&tmpu32, 4, 1, f);
            fseek(f, *doff, SEEK_SET);
            paftool_create_wrdirs(f, &n->dir.contents, userdata, doff);
        }
        tmpdoff += 13;
        fseek(f, tmpdoff, SEEK_SET);
    }
}
static void paftool_create_wrfiles(FILE* f, struct ftree* tree, struct ftreeuserdata* userdata, uint32_t doff, uint32_t* foff) {
    if (!tree->len) return;
    doff += 25;
    fseek(f, doff, SEEK_SET);
    for (size_t i = 0; i < tree->len; ++i) {
        struct ftree_node* n = &tree->data[i];
        if (!n->isdir) {
            uint32_t tmpu32 = swaple32(*foff);
            fwrite(&tmpu32, 4, 1, f);
            fseek(f, *foff, SEEK_SET);
            long size = n->file.size;
            tmpu32 = swaple32(size);
            fwrite(&tmpu32, 4, 1, f);
            fwrite(&tmpu32, 4, 1, f);
            fwrite(&tmpu32, 4, 1, f);
            tmpu32 = 0;
            fwrite(&tmpu32, 4, 1, f);
            *foff += size + 16;
            FILE* tmpf = fopen(n->file.path, "rb");
            static char buf[256];
            while (size) {
                long datalen = (size >= (long)sizeof(buf)) ? (long)sizeof(buf) : size;
                fread(buf, 1, datalen, tmpf);
                fwrite(buf, 1, datalen, f);
                size -= datalen;
            }
            fclose(tmpf);
        }
        doff += 13;
        fseek(f, doff, SEEK_SET);
    }
    for (size_t i = 0; i < tree->len; ++i) {
        struct ftree_node* n = &tree->data[i];
        if (n->isdir) paftool_create_wrfiles(f, &n->dir.contents, userdata, userdata->data[(uintptr_t)n->userdata], foff);
    }
}

int paftool_create(char* argv0, int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "%s: Missing 'OUTPUT' param for 'create' command\n", argv0);
        return 1;
    }
    FILE* f = fopen(argv[0], "w+b");
    if (!f) {
        fprintf(stderr, "%s: Could not open output: %s\n", argv0, strerror(errno));
        return 1;
    }
    fputc('P', f);
    fputc('A', f);
    fputc('F', f);
    fputc(PAF_VER, f);
    if (argc == 1) {
        puts("Creating archive optimized for writing...");
        static const unsigned freespaceslots = 16;
        static const unsigned rootdirentries = 16;
        uint32_t tmpu32 = swaple32(24 + freespaceslots * 8);
        fwrite(&tmpu32, 4, 1, f);
        tmpu32 = swaple32(freespaceslots);
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        tmpu32 = 0;
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        for (unsigned i = 0; i < freespaceslots; ++i) {
            fwrite(&tmpu32, 4, 1, f);
            fwrite(&tmpu32, 4, 1, f);
        }
        tmpu32 = swaple32(rootdirentries);
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        tmpu32 = 0;
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        for (unsigned i = 0; i < rootdirentries; ++i) {
            fputc(0, f);
            fwrite(&tmpu32, 4, 1, f);
            fwrite(&tmpu32, 4, 1, f);
            fwrite(&tmpu32, 4, 1, f);
        }
        puts("Done");
    } else {
        puts("Creating archive optimized for size...");
        uint32_t tmpu32 = swaple32(24);
        fwrite(&tmpu32, 4, 1, f);
        tmpu32 = 0;
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        fwrite(&tmpu32, 4, 1, f);
        //puts("Scanning files...");
        struct ftree tree;
        ftree_init(&tree, argc - 1, argv + 1);
        //ftree_list(&tree);
        tmpu32 = 24;
        struct ftreeuserdata userdata;
        VLB_INIT(userdata, 16, VLB_OOM_NOP);
        paftool_create_wrdirs(f, &tree, &userdata, &tmpu32);
        paftool_create_wrfiles(f, &tree, &userdata, 24, &tmpu32);
        VLB_FREE(userdata);
        ftree_free(&tree);
        puts("Done");
    }
    fclose(f);
    return 0;
}
