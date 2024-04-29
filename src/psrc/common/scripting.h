#ifndef PSRC_COMMON_SCRIPTING
#define PSRC_COMMON_SCRIPTING

#include "string.h"
#include "threading.h"

#include <stdint.h>
#include <stdbool.h>

enum __attribute__((packed)) pbtype {
    PBTYPE__SPECIAL = -1,
    PBTYPE_VOID,
    PBTYPE_BOOL,
    PBTYPE_I8,
    PBTYPE_I16,
    PBTYPE_I32,
    PBTYPE_I64,
    PBTYPE_U8,
    PBTYPE_U16,
    PBTYPE_U32,
    PBTYPE_U64,
    PBTYPE_F32,
    PBTYPE_F64,
    PBTYPE_VEC,
    PBTYPE_STR,
};

struct pbdata_vec {
    float x;
    float y;
    float z;
};
union pbdata {
    uint8_t b;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    float f32;
    double f64;
    struct pbdata_vec vec;
    struct charbuf str;
    struct {
        union {
            uint8_t* b; // packed bitmap
            int8_t* i8;
            int16_t* i16;
            int32_t* i32;
            int64_t* i64;
            uint8_t* u8;
            uint16_t* u16;
            uint32_t* u32;
            uint64_t* u64;
            float* f32;
            double* f64;
            struct pbdata_vec* vec;
            struct charbuf* str;
        };
        unsigned len;
        unsigned size;
        uint8_t copied : 1;
    } array;
};

struct pbp {
    
};

struct pbvm;

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

typedef void (*pb_cfunc)(struct pbvm*, void*, struct pbvm_ccall_arg*, struct pbvm_ccall_ret**);
typedef bool (*pb_external)(struct pbp*, char*);

enum __attribute__((packed)) pbvm_op {
    PBVM_FRAME,     // Push a frame marker
    PBVM_PUSHCONST, // Push a constant
    PBVM_PUSHVAR,   // Push a variable
    PBVM_PUSHARG,   // Push an argument
    PBVM_DIM,       // Set up a var
    PBVM_SET,       // Set a var
    PBVM_DEL,       // Delete a var
    PBVM_DEFSUB,    // Define a sub
    PBVM_DELSUB,    // Delete a sub
    PBVM_ON,        // Subscribe to an event
    PBVM_UNON,      // Unsubscribe from an event
    PBVM_JSR,       // Jump to a sub (GOSUB)
    PBVM_RET,       // Return from a sub
    PBVM_CCALL,     // Call a C function
    PBVM_JMP,       // Jump
    PBVM_B,         // Branch
    PBVM_ADD,       // Add (+)
    PBVM_SUB,       // Subtract (-)
    PBVM_MUL,       // Multiply (*)
    PBVM_DIV,       // Divide (/)
    PBVM_REM,       // Remainder (%)
    PBVM_POW,       // Power (POW())
    PBVM_SIN,       // Sine (SIN())
    PBVM_COS,       // Cosine (COS())
    PBVM_TAN,       // Tangent (TAN())
    PBVM_GT,        // Greater than (>)
    PBVM_GE,        // Gearter or equal (>=)
    PBVM_EQ,        // Equal (=)
    PBVM_LT,        // Less than (<)
    PBVM_LE,        // Less or equal (<=)
    PBVM_NOT,       // Logical not (!)
    PBVM_BAND,      // Bitwise AND (&)
    PBVM_BOR,       // Bitwise OR (|)
    PBVM_BXOR,      // Bitwise XOR (^)
    PBVM_BNOT,      // Bitwise NOT (~)
    PBVM_CASTBOOL,  // Cast to BOOL
    PBVM_CASTI8,    // Cast to I8
    PBVM_CASTI16,   // Cast to I16
    PBVM_CASTI32,   // Cast to I32
    PBVM_CASTI64,   // Cast to I64
    PBVM_CASTU8,    // Cast to U8
    PBVM_CASTU16,   // Cast to U16
    PBVM_CASTU32,   // Cast to U32
    PBVM_CASTU64,   // Cast to U64
    PBVM_CASTF32,   // Cast to F32
    PBVM_CASTF64,   // Cast to F64
    PBVM_CASTVEC,   // Cast to VEC
    PBVM_CASTSTR,   // Cast to STR
    PBVM_OUTCONST,  // Write a constant to the output
    PBVM_OUTVAR,    // Write a variable to the output
    PBVM_OUTARG,    // Write an argument to the output
    PBVM_OUTTAB,    // Write a tab to the output
    PBVM_OUTNL,     // Write a newline to the output
    PBVM_EXIT,      // Exit
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
struct pbvm_sub {
    struct {
        uint8_t copied : 1;
    };
    char* name;
    uint32_t namecrc;
    enum pbtype rettype;
    unsigned argcount;
    enum pbtype* argtypes;
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

struct pv_varref {
    char* name;
    uint32_t namecrc;
    struct {
        uint8_t indata : 1;
    };
    unsigned datapos;
};
struct pv_subref {
    char* name;
    uint32_t namecrc;
};

struct script {
    unsigned oplen;
    void* ops;
    unsigned datalen;
    void* data;
    unsigned varcount;
    struct pb_varref* vars;
    unsigned subcount;
    struct pb_subref* subs;
};

struct pbvm {
    struct script* script;
    struct {
        struct pbvm_var* data;
        unsigned len;
        unsigned size;
        struct {
            unsigned* data;
            unsigned len;
            unsigned size;
        } sizelist;
        unsigned* map;
    } vars;
    struct {
        struct pbvm_sub* data;
        unsigned len;
        unsigned size;
        unsigned* map;
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
        unsigned* data;
        unsigned len;
        unsigned size;
        unsigned base;
    } args;
};

#endif
