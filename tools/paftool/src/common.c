#include "common.h"

#include <psrc/crc.h>
#include <psrc/string.h>
#include <psrc/filesystem.h>

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#ifndef _WIN32
    #include <sys/stat.h>
#else
    #include <windows.h>
#endif

static void ftree_lsdir(struct ftree* tree, struct lsstate* ls) {
    const char* name;
    const char* path;
    while (getls(ls, &name, &path)) {
        uint32_t namecrc = strcrc32(name);
        for (size_t i = 0; i < tree->len; ++i) {
            struct ftree_node* n = &tree->data[i];
            if (n->namecrc == namecrc && !strcmp(n->name, name)) goto noadd;
        }
        int isfile = isFile(path);
        if (isfile < 0) {
            fprintf(stderr, "Warning: Could not stat '%s': %s; ignoring\n", path, strerror(errno));
            continue;
        }
        if (isfile > 1) {
            fprintf(stderr, "Warning: Ignoring '%s' as it is a special file; ignoring\n", path);
            continue;
        }
        struct ftree_node* n;
        if (isfile) {
            FILE* f = fopen(path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fclose(f);
                VLB_NEXTPTR(*tree, n, 2, 1, VLB_OOM_NOP);
                n->isdir = 0;
                n->name = strdup(name);
                n->namecrc = namecrc;
                n->file.path = strdup(path);
                n->file.size = size;
            } else {
                fprintf(stderr, "Warning: Could not open '%s'; ignoring\n", path);
            }
        } else {
            struct lsstate subls;
            if (startls(path, &subls)) {
                VLB_NEXTPTR(*tree, n, 2, 1, VLB_OOM_NOP);
                n->isdir = 1;
                n->name = strdup(name);
                n->namecrc = namecrc;
                VLB_INIT(n->dir.contents, 8, VLB_OOM_NOP);
                ftree_lsdir(&n->dir.contents, &subls);
                endls(&subls);
            } else {
                fprintf(stderr, "Warning: Could not list '%s'; ignoring\n", path);
            }
        }
        noadd:;
    }
}
void ftree_init(struct ftree* tree, int ct, char** pl) {
    VLB_INIT(*tree, 16, VLB_OOM_NOP);
    for (int i = 0; i < ct; ++i) {
        const char* p = pl[i];
        int isfile = isFile(p);
        if (isfile < 0) {
            fprintf(stderr, "Warning: Could not stat '%s': %s; ignoring\n", p, strerror(errno));
            continue;
        }
        if (isfile > 1) {
            fprintf(stderr, "Warning: Ignoring '%s' as it is a special file; ignoring\n", p);
            continue;
        }
        union {
            struct {
                struct lsstate ls;
            } dir;
            struct {
                long size;
            } file;
        } nodedata;
        if (isfile) {
            FILE* f = fopen(p, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                nodedata.file.size = ftell(f);
                fclose(f);
            } else {
                fprintf(stderr, "Warning: Could not open '%s'; ignoring\n", p);
                continue;
            }
        } else {
            if (!startls(p, &nodedata.dir.ls)) {
                fprintf(stderr, "Warning: Could not list '%s'; ignoring\n", p);
                continue;
            }
        }
        size_t namect;
        char** names;
        names = splitstr(p, PATHSEPSTR, false, &namect);
        char* freenames = *names;
        {
            size_t offset = 0;
            for (size_t j = 0; j < namect; ++j) {
                if (!strcmp(names[j], "..")) offset = j + 1;
            }
            if (offset) {
                fputs("Warning: Trimming '", stderr);
                for (size_t j = 0; j < offset; ++j) {
                    fputs(names[j], stderr);
                    fputc(PATHSEP, stderr);
                }
                fprintf(stderr, "' out of '%s'\n", p);
                names += offset;
                namect -= offset;
            }
            size_t j = 0;
            while (1) {
                if (j == namect) break;
                if (!*names[j] || !strcmp(names[j], ".")) {
                    for (size_t k = j; k < namect - 1; ++k) {
                        names[k] = names[k + 1];
                    }
                    --namect;
                    continue;
                }
                ++j;
            }
        }
        struct ftree* curtree = tree;
        struct ftree_node* n;
        if (namect > 1) {
            for (size_t j = 0; j < namect - 1; ++j) {
                uint32_t namecrc = strcrc32(names[j]);
                for (size_t nodei = 0; nodei < curtree->len; ++nodei) {
                    n = &curtree->data[nodei];
                    if (n->namecrc == namecrc && !strcmp(n->name, names[j])) {
                        if (!n->isdir) {
                            fputs("Warning: Archive path '", stderr);
                            fputs(names[0], stderr);
                            for (size_t k = 1; k <= j; ++k) {
                                fputc(PATHSEP, stderr);
                                fputs(names[k], stderr);
                            }
                            fputs("' is not a dir; overriding", stderr);
                            n->isdir = 1;
                            VLB_INIT(n->dir.contents, 1, VLB_OOM_NOP);
                        }
                        curtree = &n->dir.contents;
                        goto noadd;
                    }
                }
                VLB_NEXTPTR(*curtree, n, 2, 1, VLB_OOM_NOP);
                n->isdir = 1;
                n->name = strdup(names[j]);
                n->namecrc = namecrc;
                VLB_INIT(n->dir.contents, 1, VLB_OOM_NOP);
                curtree = &n->dir.contents;
                noadd:;
            }
        }
        if (isfile) {
            const char* name = names[namect - 1];
            uint32_t namecrc = strcrc32(name);
            for (size_t nodei = 0; nodei < curtree->len; ++nodei) {
                n = &curtree->data[nodei];
                if (n->namecrc == namecrc && !strcmp(n->name, name)) {
                    fputs("Warning: Archive path '", stderr);
                    fputs(names[0], stderr);
                    for (size_t j = 1; j < namect; ++j) {
                        fputc(PATHSEP, stderr);
                        fputs(names[j], stderr);
                    }
                    if (n->isdir) {
                        fputs("' already exists as a dir; ignoring", stderr);
                        goto file_ignore;
                    } else {
                        fputs("' already exists; overriding", stderr);
                        free(n->name);
                        goto file_noadd;
                    }
                }
            }
            VLB_NEXTPTR(*curtree, n, 2, 1, VLB_OOM_NOP);
            n->isdir = 0;
            file_noadd:;
            n->name = strdup(name);
            n->namecrc = namecrc;
            n->file.path = strdup(p);
            n->file.size = nodedata.file.size;
            file_ignore:;
        } else {
            if (namect) {
                const char* name = names[namect - 1];
                uint32_t namecrc = strcrc32(name);
                for (size_t nodei = 0; nodei < curtree->len; ++nodei) {
                    n = &curtree->data[nodei];
                    if (n->namecrc == namecrc && !strcmp(n->name, name)) {
                        if (!n->isdir) {
                            fputs("Warning: Archive path '", stderr);
                            fputs(names[0], stderr);
                            for (size_t j = 1; j < namect; ++j) {
                                fputc(PATHSEP, stderr);
                                fputs(names[j], stderr);
                            }
                            fputs("' is not a dir; overriding", stderr);
                            n->isdir = 1;
                            VLB_INIT(n->dir.contents, 1, VLB_OOM_NOP);
                        }
                        curtree = &n->dir.contents;
                        goto dir_noadd;
                    }
                }
                VLB_NEXTPTR(*curtree, n, 2, 1, VLB_OOM_NOP);
                n->isdir = 1;
                n->name = strdup(name);
                n->namecrc = namecrc;
                VLB_INIT(n->dir.contents, 8, VLB_OOM_NOP);
                curtree = &n->dir.contents;
                dir_noadd:;
            }
            ftree_lsdir(curtree, &nodedata.dir.ls);
            endls(&nodedata.dir.ls);
        }
        free(freenames);
    }
}
void ftree_free(struct ftree* tree) {
    VLB_FREE(*tree);
}

static void ftree_list_internal(struct ftree* tree, int depth) {
    for (size_t i = 0; i < tree->len; ++i) {
        struct ftree_node* n = &tree->data[i];
        for (int i = 0; i < depth; ++i) {
            fputs("  ", stdout);
        }
        putchar('\'');
        fputs(n->name, stdout);
        fputs("' (", stdout);
        if (n->isdir) {
            fputs("directory; ", stdout);
            printf("%" PRIuPTR " item%s)\n", n->dir.contents.len, (n->dir.contents.len != 1) ? "s" : "");
            ftree_list_internal(&n->dir.contents, depth + 1);
        } else {
            fputs("file; ", stdout);
            printf("%ld byte%s) -> '%s'\n", n->file.size, (n->file.size != 1) ? "s" : "", n->file.path);
        }
    }
}
void ftree_list(struct ftree* t) {
    ftree_list_internal(t, 0);
}
