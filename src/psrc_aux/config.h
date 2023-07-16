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

struct cfg* cfg_open(char* path);
char* cfg_getvar(struct cfg*, char* sect, char* var);
bool cfg_getvarto(struct cfg*, char* sect, char* var, char* data, size_t size);
void cfg_setvar(struct cfg*, char* sect, char* var, bool overwrite);
void cfg_delvar(struct cfg*, char* sect, char* var);
void cfg_write(struct cfg*, char* path);
void cfg_close(struct cfg*);

#endif
