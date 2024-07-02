#ifndef PSRC_COMMON_PBVM_H
#define PSRC_COMMON_PBVM_H

#include "pbasic.h"

enum __attribute__((packed)) pbvm_op {
    PBVM_OP_NOP,       // Padding
    PBVM_OP_HALT,      // Exit program with a return value of 0
    PBVM_OP_FRAME,     // Push a frame marker (pushes: marker)
    PBVM_OP_PUSHCONST, // Push a constant (u32 index, u32 dim; pops: [index]...; pushes: value)
    PBVM_OP_PUSHVAR,   // Push a variable (u32 index, u32 dim; pops: [index]...; pushes: value)
    PBVM_OP_PUSHLOCAL, // Push a local variable (u32 index, u32 dim; pops: [index]...; pushes: value)
    PBVM_OP_PUSHARG,   // Push an argument (u32 offset, u32 dim; pops: [index]...; pushes: value)
    PBVM_OP_POP,       // Pop a value off the stack
    PBVM_OP_DIM,       // Set up a var (u32 index, u32 dims; pops: size...)
    PBVM_OP_DIMLOCAL,  // Set up a local var (u32 index, u32 dims; pops: size...)
    PBVM_OP_SET,       // Set a var (u32 index; pops: value)
    PBVM_OP_DEL,       // Delete a var (u32 index)
    PBVM_OP_DEFSUB,    // Begin a sub definition (u32 index, u8 rettype, u32 retdim, u32 argc, {u8 type, u32 dim} info[])
    PBVM_OP_ENDSUB,    // End a sub definition
    PBVM_OP_DELSUB,    // Delete a sub (u32 index)
    PBVM_OP_ON,        // Subscribe to an event (u32 subindex; pops: name)
    PBVM_OP_UNON,      // Unsubscribe from an event (pops: name)
    PBVM_OP_JSR,       // Jump to a sub and discard the return value (GOSUB) (u32 index; pops: args...; pushes: retval)
    PBVM_OP_JSRR,      // Jump to a sub and push the return value (GOSUB()) (u32 index; pops: args...; pushes: retval)
    PBVM_OP_RET,       // Return from a sub (pops: retval)
    PBVM_OP_CCALL,     // Call a C function (pb_cfunc func, u32 datasz, u8 data[]; pops: args...; pushes: retval)
    PBVM_OP_JMP,       // Jump (i32 offset)
    PBVM_OP_B,         // Branch (i32 trueoff, i32 falseoff) (pops: cond)
    PBVM_OP_ADD,       // Add (+) (pops: to, value; pushes: result)
    PBVM_OP_SUB,       // Subtract (-) (pops: from, value; pushes: result)
    PBVM_OP_MUL,       // Multiply (*) (pops: value, by; pushes: result)
    PBVM_OP_DIV,       // Divide (/) (pops: value, by; pushes: result)
    PBVM_OP_REM,       // Remainder (%) (pops: value, by; pushes: result)
    PBVM_OP_POW,       // Power (POW()) (pops: value, by; pushes: result)
    PBVM_OP_SIN,       // Sine (SIN()) (pops: in; pushes: result)
    PBVM_OP_COS,       // Cosine (COS()) (pops: in; pushes: result)
    PBVM_OP_TAN,       // Tangent (TAN()) (pops: in; pushes: result)
    PBVM_OP_GT,        // Greater than (>) (pops: left, right; pushes: result)
    PBVM_OP_GE,        // Greater or equal (>=) (pops: left, right; pushes: result)
    PBVM_OP_EQ,        // Equal (=) (pops: value, to; pushes: result)
    PBVM_OP_LT,        // Less than (<) (pops: left, right; pushes: result)
    PBVM_OP_LE,        // Less or equal (<=) (pops: left, right; pushes: result)
    PBVM_OP_NOT,       // Logical not (!) (pops: cond; pushes: result)
    PBVM_OP_BAND,      // Bitwise AND (&) (pops: value, with; pushes: result)
    PBVM_OP_BOR,       // Bitwise OR (|) (pops: value, with; pushes: result)
    PBVM_OP_BXOR,      // Bitwise XOR (^) (pops: value, with; pushes: result)
    PBVM_OP_BNOT,      // Bitwise NOT (~) (pops: value; pushes: result)
    PBVM_OP_CAST,      // Cast to another type (u8 type, u32 dims; pops: value or value, value, value; pushes: result)
    PBVM_OP_OUT,       // Pop a value off the stack and write it to the output (pops: value)
    PBVM_OP_OUTTAB,    // Write a tab to the output
    PBVM_OP_OUTNL,     // Write a newline to the output
    PBVM_OP_EXIT       // Exit (pops: retval)
};

struct pbvm_ccall_arg {
    union pbdata data;
    enum pbtype type;
    unsigned dim;
    union {
        unsigned size;
        unsigned* sizes;
    };
};
struct pbvm_ccall_ret {
    union pbdata data;
    enum pbtype type;
    unsigned dim;
    unsigned sizes[];
};

struct pbvm_var {
    union pbdata data;
    char* name;
    uint32_t namecrc;
    enum pbtype type;
    unsigned dim;
    union {
        unsigned size;
        unsigned sizeindex;
    };
};
struct pbvm_local {
    union pbdata data;
    enum pbtype type;
    unsigned dim;
    union {
        unsigned size;
        unsigned sizeindex;
    };
};
struct pbvm_sub_type {
    enum pbtype type;
    unsigned dim;
    union {
        unsigned size;
        unsigned sizeindex;
    };
};
struct pbvm_sub {
    int script; // -1 for active, 0+ for duped
    int offset;
    struct {
        uint8_t copied : 1;
        uint8_t deleted : 1;
        uint8_t evhandler : 1;
    };
    unsigned uses;
    char* name;
    uint32_t namecrc;
    struct pbvm_sub_type rettype;
    unsigned argcount;
    struct pbvm_sub_type* argtypes;
};
struct pbvm_tmp {
    union pbdata data;
    enum pbtype type;
    unsigned dim;
    union {
        unsigned size;
        unsigned sizeindex;
    };
};
enum __attribute__((packed)) pbvm_stackelem_type {
    PBVM_SE_TYPE_FRAME,
    PBVM_SE_TYPE_CONST,
    PBVM_SE_TYPE_VAR,
    PBVM_SE_TYPE_LOCAL,
    PBVM_SE_TYPE_TMP
};
struct pbvm_stackelem {
    enum pbvm_stackelem_type type;
    unsigned index;
};
struct pbvm_script {
    struct rc_script* script;
    int* varmap; // -1 for unmapped/deleted
    int* submap; // -1 for unmapped/deleted
};
struct pbvm_state {
    int sb;
    int sp;
    int pc;
    struct {
        struct pbvm_local* data;
        unsigned len;
        unsigned size;
        struct {
            unsigned* data;
            unsigned len;
            unsigned size;
        } sizelist;
    } locals;
};
struct pbvm {
    struct {
        struct pbvm_script* data;
        unsigned len;
        unsigned size;
        unsigned current;
    } scripts;
    struct {
        struct pbvm_var* data;
        unsigned len;
        unsigned size;
        struct {
            unsigned* data;
            unsigned len;
            unsigned size;
        } sizelist;
    } vars;
    struct {
        struct pbvm_sub* data;
        unsigned len;
        unsigned size;
    } subs;
    struct {
        struct pbvm_tmp* data;
        unsigned len;
        unsigned size;
        struct {
            unsigned* data;
            unsigned len;
            unsigned size;
        } sizelist;
    } tmp;
    struct {
        struct pbvm_stackelem* data;
        unsigned len;
        unsigned size;
    } stack;
};

struct pbvm_eventtable_sub {
    unsigned vm;
    unsigned script;
    unsigned index;
};
struct pbvm_eventtable_event {
    char* name;
    char* namelen;
    uint32_t namecrc;
    struct {
        struct pbvm_eventtable_sub* data;
        unsigned len;
        unsigned size;
    } subs;
};
struct pbvm_eventtable {
    struct {
        struct pbvm* data;
        unsigned len;
        unsigned size;
    } vms;
    struct {
        struct pbvm_eventtable_event* data;
        unsigned len;
        unsigned size;
    } events;
};

#endif
