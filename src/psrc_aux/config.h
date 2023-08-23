#ifndef AUX_CONFIG_H
#define AUX_CONFIG_H

#include "threading.h"

#include <stdbool.h>
#include <stddef.h>

struct cfg_var {
    char* name;
    char* data;
};

struct cfg_sect {
    char* name;
    int varcount;
    struct cfg_var* vardata;
};

struct cfg {
    char* path;
    bool changed;
    int sectcount;
    struct cfg_sect* sectdata;
    mutex_t lock;
};

struct cfg* cfg_open(const char* path);
bool cfg_merge(struct cfg*, const char* path, bool overwrite);
char* cfg_getvar(struct cfg*, const char* sect, const char* var);
bool cfg_getvarto(struct cfg*, const char* sect, const char* var, const char* data, size_t size);
void cfg_setvar(struct cfg*, const char* sect, const char* var, bool overwrite);
void cfg_delvar(struct cfg*, const char* sect, const char* var);
void cfg_write(struct cfg*, const char* path);
void cfg_close(struct cfg*);

#endif
