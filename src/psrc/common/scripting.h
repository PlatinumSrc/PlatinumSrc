#ifndef PSRC_COMMON_SCRIPTING
#define PSRC_COMMON_SCRIPTING

#include "string.h"
#include "threading.h"

#include <stdint.h>
#include <stdbool.h>

enum __attribute__((packed)) svm_op {
    SVM_OP_NOP,
    // registers
    SVM_OP_LD8,     // <u8: <4 bits: 0> <4 bits: reg>> <u8: val>
    SVM_OP_LD16,    // <u8: <4 bits: 0> <4 bits: reg>> <u16: val>
    SVM_OP_LD32,    // <u8: <4 bits: 0> <4 bits: reg>> <u32: val>
    SVM_OP_LDM8,    // <u8: <4 bits: 0> <4 bits: reg>> <u32: addr>
    SVM_OP_LDM16,   // <u8: <4 bits: 0> <4 bits: reg>> <u32: addr>
    SVM_OP_LDM32,   // <u8: <4 bits: 0> <4 bits: reg>> <u32: addr>
    SVM_OP_LDR8,    // <u8: <4 bits: reg (addr)> <4 bits: reg>>
    SVM_OP_LDR16,   // <u8: <4 bits: reg (addr)> <4 bits: reg>>
    SVM_OP_LDR32,   // <u8: <4 bits: reg (addr)> <4 bits: reg>>
    SVM_OP_STM8,    // <u8: <4 bits: 0> <4 bits: reg>> <u32: addr>
    SVM_OP_STM16,   // <u8: <4 bits: 0> <4 bits: reg>> <u32: addr>
    SVM_OP_STM32,   // <u8: <4 bits: 0> <4 bits: reg>> <u32: addr>
    SVM_OP_STR8,    // <u8: <4 bits: reg (addr)> <4 bits: reg>>
    SVM_OP_STR16,   // <u8: <4 bits: reg (addr)> <4 bits: reg>>
    SVM_OP_STR32,   // <u8: <4 bits: reg (addr)> <4 bits: reg>>
    // memory
    SVM_OP_ALLOC,   // <u8: <4 bits: reg (out)> <4 bits: reg (size)>>
    SVM_OP_REALLOC, // <u8: <4 bits: reg (size)> <4 bits: reg (addr)>>
    SVM_OP_FREE,    // <u8: <4 bits: 0> <4 bits: reg (addr)>>
    SVM_OP_MEMSET,  // <u8: <4 bits: reg (len)> <4 bits: reg (addr)>> <u8: val>
    SVM_OP_MEMCPY,  // <u8: <4 bits: reg (to)> <4 bits: reg (from)>> <u8: <4 bits: 0> <4 bits: reg (len)>>
    SVM_OP_MEMDUP,  // <u8: <4 bits: reg (len)> <4 bits: reg (addr)>> <u8: <4 bits: 0> <4 bits: reg (put)>>
    SVM_OP_STRLEN,  // <u8: <4 bits: reg (out)> <4 bits: reg (addr)>>
    SVM_OP_CRC32,   // <u8: <4 bits: reg (len)> <4 bits: reg (addr)>> <u8: <4 bits: 0> <4 bits: reg (out)>>
    SVM_OP_CRC64,   // <u8: <4 bits: reg (len)> <4 bits: reg (addr)>> <u8: <4 bits: reg (upper out)> <4 bits: reg (lower out)>>
    SVM_OP_SCRC32,  // <u8: <4 bits: reg (out)> <4 bits: reg (addr)>>
    SVM_OP_SCRC64,  // <u8: <4 bits: reg (lower out)> <4 bits: reg (addr)>> <u8: <4 bits: 0> <4 bits: reg (upper out)>>
    SVM_OP_SCCRC32, // <u8: <4 bits: reg (out)> <4 bits: reg (addr)>>
    SVM_OP_SCCRC64, // <u8: <4 bits: reg (lower out)> <4 bits: reg (addr)>> <u8: <4 bits: 0> <4 bits: reg (upper out)>>
    // stack
    SVM_OP_PUSH8,
    SVM_OP_PUSH16,
    SVM_OP_PUSH32,
    SVM_OP_POP8,
    SVM_OP_POP16,
    SVM_OP_POP32,
    // io
    SVM_OP_WRITE,   // <u8: <4 bits: reg (len)> <4 bits: reg (addr)>>
    SVM_OP_WRITES,  // <u8: <4 bits: 0> <4 bits: reg (addr)>>
    SVM_OP_EVENT,   // <u8: <4 bits: reg (name len)> <4 bits: reg (name)>> <u8: <4 bits: 0> <4 bits: reg (func)>>
    SVM_OP_FIRE,    // <u8: <4 bits: reg (name len)> <4 bits: reg (name)>> 
    // math
    SVM_OP_ADDU,
    SVM_OP_ADDI,
    SVM_OP_ADDF,
    SVM_OP_SUBU,
    SVM_OP_SUBI,
    SVM_OP_SUBF,
    SVM_OP_MULU,
    SVM_OP_MULI,
    SVM_OP_MULF,
    SVM_OP_DIVU,
    SVM_OP_DIVI,
    SVM_OP_DIVF,
    SVM_OP_REMU,
    SVM_OP_REMI,
    SVM_OP_MODF,
    SVM_OP_ABSI,
    SVM_OP_ABSF,
    SVM_OP_SINF,
    SVM_OP_COSF,
    SVM_OP_POWF,
    SVM_OP_SQRTF,
    SVM_OP_INT,
    SVM_OP_FLOAT,
    // bitwise
    SVM_OP_AND,
    SVM_OP_OR,
    SVM_OP_XOR,
    SVM_OP_NOT,
    // logic
    SVM_OP_EQ,
    SVM_OP_EQF,
    SVM_OP_GTU,
    SVM_OP_GTI,
    SVM_OP_GTF,
    SVM_OP_GEU,
    SVM_OP_GEI,
    SVM_OP_GEF,
    SVM_OP_LTU,
    SVM_OP_LTI,
    SVM_OP_LTF,
    SVM_OP_LEU,
    SVM_OP_LEI,
    SVM_OP_LEF,
    // program flow
    SVM_OP_JMP,     // <u32: addr>
    SVM_OP_JNZ,     // <u8: <4 bits: 0> <4 bits: reg>> <u32: addr>
    SVM_OP_JNZR,    // <u8: <4 bits: reg (addr)> <4 bits: reg>>
    SVM_OP_RJMP,    // <i32: offset>
    SVM_OP_RJNZ,    // <u8: <4 bits: 0> <4 bits: reg>> <i32: offset>
    SVM_OP_RJNZR,   // <u8: <4 bits: reg (offset)> <4 bits: reg>>
    SVM_OP_HALT,
    SVM_OP__COUNT
};

struct script {
    void* ops;
    void* data;
    unsigned oplen;
    unsigned datalen;
};

enum svm_error {
    SVM_ERROR_NONE,     // No error
    SVM_ERROR_ILLINST,  // Illegal instruction
    SVM_ERROR_DIVZERO,  // Divide by zero
    SVM_ERROR_MEMBOUND, // Memory operation crosses boundary
    SVM_ERROR_MEMREAD,  // Read outside of valid block
    SVM_ERROR_MEMWRITE, // Write outside of valid block
    SVM_ERROR_MEMREXEC, // Execute outside of code block
};

struct svm {
};

#endif
