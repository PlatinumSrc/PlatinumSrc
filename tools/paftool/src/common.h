#ifndef PAFTOOL_COMMON_H
#define PAFTOOL_COMMON_H

#include <common/vlb.h>

#include <stdint.h>

#ifndef _WIN32
    #define ispathsep(c) ((c) == '/')
#else
    static inline bool ispathsep(char c) {
        return (c == '/' || c == '\\');
    }
#endif

struct ftreenode {
    int type : 16;
    unsigned lsdone : 1;
    unsigned : 7;
    char* name;
    uint32_t namecrc;
    unsigned childct;
    unsigned firstchild;
    unsigned next;
    //unsigned prev;
};

struct ftree VLB(struct ftreenode);

void ftree_init(struct ftree*, const char* argv0, int ct, const char** paths);

int isFile(const char*);

#endif
