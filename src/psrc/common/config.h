#ifndef PSRC_COMMON_CONFIG_H
#define PSRC_COMMON_CONFIG_H

#ifndef PSRC_REUSEABLE
    #include "threading.h"
    #include "datastream.h"
    #define PSRC_COMMON_CONFIG_DATASTREAM struct datastream*
#else
    #include <stdio.h>
    #define PSRC_COMMON_CONFIG_DATASTREAM FILE*
    #define DS_END EOF
    #define ds_text_getc fgetc
    #define ds_text_getc_inline fgetc
    #define ds_text_getc_fullinline fgetc
    #define ds_text_ungetc(ds, c) ungetc(c, ds)
    #define ds_text_atend feof
    #define DEBUG(x) 0
#endif

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
    #if !defined(PSRC_REUSEABLE) && !defined(PSRC_NOMT)
    mutex_t lock;
    #endif
};

#ifndef PSRC_REUSEABLE
    void cfg_open(PSRC_COMMON_CONFIG_DATASTREAM, struct cfg*);
    void cfg_merge(struct cfg*, PSRC_COMMON_CONFIG_DATASTREAM, bool overwrite);
#else
    void cfg_open(FILE*, struct cfg*);
    void cfg_merge(struct cfg*, FILE*, bool overwrite);
#endif
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
