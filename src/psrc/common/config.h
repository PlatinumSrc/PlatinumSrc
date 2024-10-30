#ifndef PSRC_COMMON_CONFIG_H
#define PSRC_COMMON_CONFIG_H

#include "threading.h"
#include "datastream.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct cfg_var {
    char* name;
    uint32_t namecrc;
    char* data;
};

struct cfg_sect {
    char* name;
    uint32_t namecrc;
    int varcount;
    struct cfg_var* vardata;
};

struct cfg {
    bool changed;
    int sectcount;
    struct cfg_sect* sectdata;
    #ifndef PSRC_NOMT
    mutex_t lock;
    #endif
};

void cfg_open(struct datastream*, struct cfg*);
void cfg_merge(struct cfg*, struct datastream*, bool overwrite);
void cfg_mergemem(struct cfg*, struct cfg* from, bool overwrite);
char* cfg_getvar(struct cfg*, const char* sect, const char* var);
bool cfg_getvarto(struct cfg*, const char* sect, const char* var, char* data, size_t size);
void cfg_setvar(struct cfg*, const char* sect, const char* var, const char* data, bool overwrite);
void cfg_delvar(struct cfg*, const char* sect, const char* var);
void cfg_delsect(struct cfg*, const char* sect);
void cfg_delall(struct cfg*);
void cfg_write(struct cfg*, const char* path);
void cfg_close(struct cfg*);

#endif
