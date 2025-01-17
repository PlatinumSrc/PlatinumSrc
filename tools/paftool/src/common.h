#ifndef PAFTOOL_COMMON_H
#define PAFTOOL_COMMON_H

#include <psrc/vlb.h>

#include <stdint.h>
#include <sys/types.h>

struct ftree_node;
struct ftree VLB(struct ftree_node);

struct ftree_node {
    uint8_t isdir : 1;
    uint8_t : 7;
    char* name;
    uint32_t namecrc;
    union {
        struct {
            struct ftree contents;
        } dir;
        struct {
            char* path;
            long size;
        } file;
    };
};

#ifndef _WIN32
    #define PATHSPLITSTR "/"
#else
    #define PATHSPLITSTR "\\/"
#endif

void ftree_init(struct ftree*, int ct, char** paths);
void ftree_free(struct ftree*);
void ftree_list(struct ftree*);

#endif
