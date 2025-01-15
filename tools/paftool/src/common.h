#ifndef PAFTOOL_COMMON_H
#define PAFTOOL_COMMON_H

#include <psrc/vlb.h>

#include <stdint.h>
#include <sys/types.h>

struct ftreenode;
struct ftree VLB(struct ftreenode);

struct ftreenode {
    uint8_t isdir : 1;
    uint8_t : 7;
    char* name;
    uint32_t namecrc;
    union {
        struct {
            struct ftree contents;
        } dir;
        struct {
            off_t size;
        } file;
    };
};

void ftree_init(struct ftree*, int ct, char** paths);
void ftree_free(struct ftree*);

#endif
