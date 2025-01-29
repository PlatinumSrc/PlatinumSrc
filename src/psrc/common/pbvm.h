#ifndef PSRC_COMMON_PBVM_H
#define PSRC_COMMON_PBVM_H

#include "pbasic.h"

#include "../vlb.h"

PACKEDENUM pbvm_op {
    PBVM_OP_NOP,        // Padding
    PBVM_OP_HALT,       // Exit program with a return value of 0
    PBVM_OP_EXIT,       // Exit (pops: retval)
    PBVM_OP_ENTERSCOPE, // Enter a new scope (u32 localvarct)
    PBVM_OP_EXITSCOPE,  // Exit the current scope (u32 localvarct)
    PBVM_OP_FRAME,      // Push a frame marker (pushes: marker)
    PBVM_OP_PUSHCONST,  // Push a constant (u32 index, u32 dim; pops: [index]...; pushes: value)
    PBVM_OP_PUSHGLOBAL, // Push a global variable (u32 index, u32 dim; pops: [index]...; pushes: value)
    PBVM_OP_PUSHLOCAL,  // Push a local variable (u32 offset, u32 dim; pops: [index]...; pushes: value)
    PBVM_OP_PUSHLARG,   // Push a local argument (u32 index, u32 dim; pops: [index]...; pushes: value)
    PBVM_OP_PUSHGARG,   // Push a global argument (ARGV()) (u32 index; pops: index; pushes: string)
    PBVM_OP_PUSHGARGCT, // Push the global argument count (ARGC()) (pushes: count)
    PBVM_OP_DIMGLOBAL,  // Set up a global var (u32 index, u32 dims; pops: size...)
    PBVM_OP_DIMLOCAL,   // Set up a local var (u32 offset, u32 dims; pops: size...)
    PBVM_OP_SETGLOBAL,  // Set a global var (u32 index; pops: value)
    PBVM_OP_SETLOCAL,   // Set a local var (u32 offset; pops: value)
    PBVM_OP_DELGLOBAL,  // Delete a global var (u32 index)
    PBVM_OP_DELLOCAL,   // Delete a local var (u32 offset)
    PBVM_OP_DEFSUB,     // Define a sub (u32 index, u32 size, i32 rettype, u32 retdim, u32 argc, {i32 type, u32 dim} info[])
    PBVM_OP_DELSUB,     // Delete a sub (u32 index)
    PBVM_OP_JMP,        // Jump (i32 offset)
    PBVM_OP_B,          // Branch (i32 trueoff, i32 falseoff) (pops: cond)
    PBVM_OP_JSR,        // Jump to a sub and discard the return value (GOSUB) (u32 index, u32 argc; pops: args...)
    PBVM_OP_JSRV,       // Jump to a sub and push the return value (GOSUB()) (u32 index, u32 argc; pops: args...; pushes: retval)
    PBVM_OP_RET,        // Return from a sub without a return value
    PBVM_OP_RETV,       // Return from a sub with a return value (pops: retval)
    PBVM_OP_CCALL,      // Call a C routine (u32 id, u32 argc, u32 datasz, u8 data[]; pops: args...; pushes: retval)
    PBVM_OP_ADD,        // Add (+) (pops: to, value; pushes: result)
    PBVM_OP_SUB,        // Subtract (-) (pops: from, value; pushes: result)
    PBVM_OP_MUL,        // Multiply (*) (pops: value, by; pushes: result)
    PBVM_OP_DIV,        // Divide (/) (pops: value, by; pushes: result)
    PBVM_OP_REM,        // Remainder (%) (pops: value, by; pushes: result)
    PBVM_OP_POW,        // Power (POW()) (pops: value, by; pushes: result)
    PBVM_OP_SIN,        // Sine (SIN()) (pops: in; pushes: result)
    PBVM_OP_COS,        // Cosine (COS()) (pops: in; pushes: result)
    PBVM_OP_TAN,        // Tangent (TAN()) (pops: in; pushes: result)
    PBVM_OP_GT,         // Greater than (>) (pops: left, right; pushes: result)
    PBVM_OP_GE,         // Greater or equal (>=) (pops: left, right; pushes: result)
    PBVM_OP_EQ,         // Equal (=) (pops: value, to; pushes: result)
    PBVM_OP_LT,         // Less than (<) (pops: left, right; pushes: result)
    PBVM_OP_LE,         // Less or equal (<=) (pops: left, right; pushes: result)
    PBVM_OP_LNOT,       // Logical NOT (!) (pops: cond; pushes: result)
    PBVM_OP_BNOT,       // Bitwise NOT (~) (pops: value; pushes: result)
    PBVM_OP_AND,        // Bitwise AND (&) (pops: value, with; pushes: result)
    PBVM_OP_OR,         // Bitwise OR (|) (pops: value, with; pushes: result)
    PBVM_OP_XOR,        // Bitwise XOR (^) (pops: value, with; pushes: result)
    PBVM_OP_CAST,       // Cast to another type (i32 type, u32 dims; pops: value or value, value, value; pushes: result)
    PBVM_OP_SLEEP,      // Sleep (pops: seconds (float) or microseconds)
    PBVM_OP_OUT,        // Pop a value off the stack and write it to the output (pops: value)
    PBVM_OP_OUTTAB,     // Write a tab to the output
    PBVM_OP_OUTNL       // Write a newline to the output
};

PACKEDENUM pbvm_stackelemtype {
    PBVM_STACKELEMTYPE_FRAME,
    PBVM_STACKELEMTYPE_VALUE,
    PBVM_STACKELEMTYPE_CONST,
    PBVM_STACKELEMTYPE_GLOBAL,
    PBVM_STACKELEMTYPE_LOCAL,
    PBVM_STACKELEMTYPE_LARG,
    PBVM_STACKELEMTYPE_GARG,
    PBVM_STACKELEMTYPE_GARGCT
};

struct pbvm_script {
    struct pbscript* script;
    uint32_t* globalmap;
    uint32_t* submap;
};
struct pbvm_var {
    const char* name;
    struct pbdata data;
};
struct pbvm_sub {
    const char* name;
    const uint8_t* data;
    uint8_t copied : 1;
    uint8_t deleted : 1;
    uint8_t : 6;
};
struct pbvm_stackelem {
    enum pbvm_stackelemtype type;
    uint32_t index; // ignored for FRAME, VALUE, and GARGCT
};
struct pbvm {
    struct VLB(struct pbvm_script) scripts;
    struct VLB(struct pbvm_var) globals;
    struct VLB(struct pbvm_var) locals;
    struct VLB(struct pbvm_sub) subs;
    struct VLB(struct pbvm_stackelem) stack;
    struct VLB(struct pbdata) tmpvals;
};

struct pbvm_execopt {
    int retafter;               // return after given number of instructions, or at program end if 0
    unsigned retonsleep : 1;    // return on PBVM_OP_SLEEP
    unsigned retonjmp : 1;      // return on PBVM_OP_JMP
    unsigned retonsub : 1;      // return on PBVM_OP_JSR, PBVM_OP_JSRV, PBVM_OP_RET, and PBVM_OP_RETV
    unsigned retonb : 1;        // return on PBVM_OP_B
};

#endif
