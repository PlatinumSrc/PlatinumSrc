#ifndef AUX_CONFIG_H
#define AUX_CONFIG_H

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
};

struct cfg* cfg_open(const char* path);
char* cfg_getvar(struct cfg*, const char* sect, const char* var);
bool cfg_getvarto(struct cfg*, const char* sect, const char* var, const char* data, const size_t size);
void cfg_setvar(struct cfg*, const char* sect, const char* var, const bool overwrite);
void cfg_delvar(struct cfg*, const char* sect, const char* var);
void cfg_write(struct cfg*, const char* path);
void cfg_close(struct cfg*);

#endif
