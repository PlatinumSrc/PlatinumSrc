#ifndef PSRC_COMMON_SCRIPTING
#define PSRC_COMMON_SCRIPTING

#include "string.h"
#include "threading.h"

#include <stdint.h>
#include <stdbool.h>

enum __attribute__((packed)) pb_opcode {
    SCRIPTOPCODE__INVAL = -1,
    SCRIPTOPCODE_EXIT, // terminate execution
    SCRIPTOPCODE__COUNT,
};

struct __attribute__((packed)) pb_op {
    enum pb_opcode opcode;
    union {
    };
};

struct pbscript {
    struct pb_op* ops;
};

struct pbvar {
    //enum pbtype type;
    struct pbblock* data;
    int size;
};

struct pbvm {
    struct {
        int16_t count;
        struct pbvm_var* data;
        struct pbvm_var* bindings;
    } vars;
    struct {
        int16_t count;
        struct pbvm_sub* data;
        struct pbvm_sub* bindings;
    } subs;
    struct {
        struct pbvm_state* data;
        int size;
        int current;
        
    } states;
};

#endif
