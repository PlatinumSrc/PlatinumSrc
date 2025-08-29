#ifndef PSRC_COMMON_PBVM_H
#define PSRC_COMMON_PBVM_H

#include "pbasic.h"

#include "../vlb.h"

PACKEDENUM pbvm_op {
    PBVM_OP_NOP,        // Padding
    PBVM_OP_HALT,       // Exit program with no return value
    PBVM_OP_EXIT,       // Exit with a return value (must be a built-in type) (pops: retval)
    PBVM_OP_ENTERSCOPE, // Enter a new scope (reads: localvarct)
    PBVM_OP_EXITSCOPE,  // Exit the current scope (reads: localvarct)
    PBVM_OP_PUSHFRAME,  // Push a frame marker (pushes: marker)
    PBVM_OP_PUSHCONST,  // Push a constant (reads: index, dim; pops: [index]...; pushes: value)
    PBVM_OP_PUSHGLOBAL, // Push a global variable (reads: index, dims; pops: [index]...; pushes: value)
    PBVM_OP_PUSHLOCAL,  // Push a local variable (reads: offset, dims; pops: [index]...; pushes: value)
    PBVM_OP_PUSHLARG,   // Push a local argument (reads: index, dims; pops: [index]...; pushes: value)
    PBVM_OP_PUSHGARG,   // Push a global argument (ARGV()) (reads: index; pops: index; pushes: string)
    PBVM_OP_PUSHGARGCT, // Push the global argument count (ARGC()) (pushes: count)
    PBVM_OP_PUSHDUP,    // Clones the top value (pushes: value)
    PBVM_OP_DIMGLOBAL,  // Set up a global var (reads: index, typeid, dims; pops: size...)
    PBVM_OP_DIMLOCAL,   // Set up a local var (reads: offset, typeid, dims; pops: size...)
    PBVM_OP_SETGLOBAL,  // Set a global var (reads: index, dim; pops: [index]...; pops: value)
    PBVM_OP_SETLOCAL,   // Set a local var (reads: offset, dim; pops: [index]...; pops: value)
    PBVM_OP_DELGLOBAL,  // Delete a global var (reads: index)
    PBVM_OP_DELLOCAL,   // Delete a local var (reads: offset)
    PBVM_OP_DEFSUB,     // Define a sub (reads: id, tblindex)
    PBVM_OP_DELSUB,     // Delete a sub (reads: id)
    PBVM_OP_JMP,        // Jump (offset is within current sub) (reads: offset)
    PBVM_OP_B,          // Branch (offset is within current sub) (reads: trueoff, falseoff; pops: cond)
    PBVM_OP_JSR,        // Jump to a sub and discard the return value (GOSUB) (reads: id, argc; pops: args...)
    PBVM_OP_JSRV,       // Jump to a sub and push the return value (GOSUB()) (reads: id, argc; pops: args...; pushes: retval)
    PBVM_OP_RET,        // Return from a sub without a return value
    PBVM_OP_RETV,       // Return from a sub with a return value (pops: retval)
    PBVM_OP_CCALL,      // Call a C routine (reads: id, argc, datawordct, [data]...; pops: [args]...; pushes: retval)
    PBVM_OP_ADD,        // Add (+) (pops: to, value; pushes: result)
    PBVM_OP_SUB,        // Subtract (-) (pops: from, value; pushes: result)
    PBVM_OP_MUL,        // Multiply (*) (pops: value, by; pushes: result)
    PBVM_OP_DIV,        // Divide (/) (pops: value, by; pushes: result)
    PBVM_OP_REM,        // Remainder (%) (pops: value, by; pushes: result)
    PBVM_OP_POW,        // Power (POW()) (pops: value, by; pushes: result)
    PBVM_OP_SIN,        // Sine (SIN()) (pops: in; pushes: result)
    PBVM_OP_COS,        // Cosine (COS()) (pops: in; pushes: result)
    PBVM_OP_TAN,        // Tangent (TAN()) (pops: in; pushes: result)
    PBVM_OP_EQ,         // Equal (==) (pops: value, to; pushes: result)
    PBVM_OP_APEQ,       // Approximately equal (~=) (pops: value, to; pushes: result)
    PBVM_OP_GT,         // Greater than (>) (pops: left, right; pushes: result)
    PBVM_OP_GE,         // Greater or equal (>=) (pops: left, right; pushes: result)
    PBVM_OP_LT,         // Less than (<) (pops: left, right; pushes: result)
    PBVM_OP_LE,         // Less or equal (<=) (pops: left, right; pushes: result)
    PBVM_OP_TYPEEQ,     // Type is equal to type ID (reads: typeid; pops: value; pushes: cond)
    PBVM_OP_TYPEAPEQ,   // Type is approximately equal to type ID (reads: typeid; pops: value; pushes: cond)
    PBVM_OP_LNOT,       // Logical NOT (!) (pops: cond; pushes: result)
    PBVM_OP_BNOT,       // Bitwise NOT (~) (pops: value; pushes: result)
    PBVM_OP_AND,        // Bitwise AND (&) (pops: value, with; pushes: result)
    PBVM_OP_OR,         // Bitwise OR (|) (pops: value, with; pushes: result)
    PBVM_OP_XOR,        // Bitwise XOR (^) (pops: value, with; pushes: result)
    PBVM_OP_CAST,       // Cast to another type (reads: typeid; pops: value; pushes: result)
    PBVM_OP_SLEEP,      // Sleep (pops: seconds (f32/f64) or microseconds)
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

struct pbvm_exec_opt {
    uint32_t mininst;           // min amount of inst to run before considering return flags (0 for inf)
    uint32_t maxinst;           // max amount of inst to run before returning (0 for inf)
    unsigned retonsleep : 1;    // return on PBVM_OP_SLEEP
    unsigned retonjmp : 1;      // return on PBVM_OP_JMP
    unsigned retonsub : 1;      // return on PBVM_OP_JSR, PBVM_OP_JSRV, PBVM_OP_RET, and PBVM_OP_RETV
    unsigned retonb : 1;        // return on PBVM_OP_B
};

#endif
