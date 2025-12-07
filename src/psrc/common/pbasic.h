#ifndef PSRC_COMMON_PBASIC_H
#define PSRC_COMMON_PBASIC_H

#include "../string.h"
#include "../threading.h"
#include "../attribs.h"
#include "../vlb.h"
#include "../datastream.h"

#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

struct pbasic;
struct pb_rodata;
struct pb_compiler;
struct pb_compiler_tok;

enum pb_error {
    PB_ERROR_NONE,
    PB_ERROR_SYNTAX,
    PB_ERROR_TYPE,
    PB_ERROR_DEF,
    PB_ERROR_INDEX,
    PB_ERROR_VALUE,
    PB_ERROR_ARG,
    PB_ERROR_MEMORY,
    PB_ERROR_INTERNAL,
    PB_ERROR__COUNT
};

PACKEDENUM pb_op {
    PB_OP_NOP,          // Padding
    PB_OP_HALT,         // Exit program with no return value
    PB_OP_EXIT,         // Exit with a return value (pops: retval)
    PB_OP_ENTERSCOPE,   // Enter a new scope (reads: localvarct)
    PB_OP_RESIZESCOPE,  // Resizes the current scope (reads: localvarct)
    PB_OP_EXITSCOPE,    // Exit the current scope
    PB_OP_DIMGLOBAL,    // Set up a global var (reads: id, typeid, dims; pops: [size]...)
    PB_OP_REDIMGLOBAL,  // Resize or redefine a global var (reads: id, typeid, dims; pops: [size]...)
    PB_OP_DIMLOCAL,     // Set up a local var or arg (reads: offset, typeid, dims; pops: [size]...)
    PB_OP_REDIMLOCAL,   // Resize or redefine a local var or arg (reads: offset, typeid, dims; pops: [size]...)
    PB_OP_PUSHCONST,    // Push a constant (reads: id; pushes: obj)
    PB_OP_PUSHGLOBAL,   // Push a global variable (reads: id; pushes: obj)
    PB_OP_PUSHLOCAL,    // Push a local variable or argument (reads: offset; pushes: obj)
    PB_OP_PUSHGARG,     // Push a global arg (ARGV(...)) (pops: index; pushes: obj)
    PB_OP_PUSHGARGCT,   // Push the global arg count (ARGC()) (pushes: obj)
    PB_OP_POP,          // Remove items from the stack (reads: count; pops: elem)
    PB_OP_REF,          // Creates a reference (@) (pops: obj; pushes: ref)
    PB_OP_DEREF,        // Dereferences a reference ($) (pops: obj; pushes: obj)
    PB_OP_DEREFCOLL,    // Dereferences a reference or expands a collection ($) (pops: obj; pushes: obj)
    PB_OP_INDEX,        // Creates an index obj from the index or adds the index to an existing index obj (reads: index; pops: obj; pushes: obj)
    PB_OP_MEMB,         // Gets a member of a COMPLEX (reads: membid; pops: obj; pushes: obj)
    PB_OP_DUP,          // Pushes a dup (pushes: dup)
    PB_OP_FRAME,        // Pushes a frame marker (pushes: framemrkr)
    PB_OP_SET,          // Set an obj (pops: value, obj)
    PB_OP_DELGLOBAL,    // Delete a global var (reads: index)
    PB_OP_DELLOCAL,     // Delete a local var or arg (reads: offset)
    PB_OP_JMP,          // Jump (reads: offset)
    PB_OP_B,            // Branch (reads: trueoff, falseoff; pops: cond)
    PB_OP_JMPI,         // Indexed jump (reads: minval, maxval, offset...; pops: index)
    PB_OP_JSR,          // Jump to a sub (GOSUB(...)) (reads: id; pops: args..., framemrkr; pushes: retval)
    PB_OP_JSRI,         // Jump to a sub on the stack (GOSUBH(...)) (pops: args..., framemrkr, sub; pushes: retval)
    PB_OP_RET,          // Return from a sub without a return value
    PB_OP_RETV,         // Return from a sub with a return value (pops: retval)
    PB_OP_CCALL,        // Call a C routine (reads: id, datawordct, [data]...; pops: [...]...; pushes: [...]...)
    PB_OP_LEN,          // Get the length of an array or the number of objects in a collection object (LEN(...)) (pops: obj; pushes: length)
    PB_OP_ADD,          // Add (+) (pops: right, left; pushes: result)
    PB_OP_SUB,          // Subtract (-) (pops: right, left; pushes: result)
    PB_OP_MUL,          // Multiply (*) (pops: right, left; pushes: result)
    PB_OP_DIV,          // Divide (/) (pops: right, left; pushes: result)
    PB_OP_REM,          // Remainder (%) (pops: right, left; pushes: result)
    PB_OP_POW,          // Power (POW()) (pops: exp, base; pushes: result)
    PB_OP_ABS,          // Sine (ABS()) (pops: in; pushes: result)
    PB_OP_CEIL,         // Ceiling (CEIL()) (pops: in; pushes: result)
    PB_OP_FLOOR,        // Floor (FLOOR()) (pops: in; pushes: result)
    PB_OP_MIN,          // Minimum (MIN()) (pops: in; pushes: result)
    PB_OP_MAX,          // Maximum (MAX()) (pops: in; pushes: result)
    PB_OP_ROUND,        // Round (ROUND()) (pops: in; pushes: result)
    PB_OP_SIN,          // Sine (SIN()) (pops: in; pushes: result)
    PB_OP_COS,          // Cosine (COS()) (pops: in; pushes: result)
    PB_OP_TAN,          // Tangent (TAN()) (pops: in; pushes: result)
    PB_OP_STR,          // Stringify (STR()) (pops: padchar, padto, uppercase, base, val; pushes: result)
    PB_OP_EQ,           // Equal (==) (pops: right, left; pushes: result)
    PB_OP_APEQ,         // Approximately equal (~=) (pops: right, left; pushes: result)
    PB_OP_GT,           // Greater than (>) (pops: right, left; pushes: result)
    PB_OP_GE,           // Greater or equal (>=) (pops: right, left; pushes: result)
    PB_OP_LT,           // Less than (<) (pops: right, left; pushes: result)
    PB_OP_LE,           // Less or equal (<=) (pops: right, left; pushes: result)
    PB_OP_LNOT,         // Logical NOT (!) (pops: cond; pushes: result)
    PB_OP_BNOT,         // Bitwise NOT (~) (pops: value; pushes: result)
    PB_OP_AND,          // Bitwise AND (&) (pops: right, left; pushes: result)
    PB_OP_OR,           // Bitwise OR (|) (pops: right, left; pushes: result)
    PB_OP_XOR,          // Bitwise XOR (^) (pops: value, with; pushes: result)
    PB_OP_CAST,         // Cast to another type (reads: typeid; pops: value; pushes: result)
    PB_OP_TYPEEQ,       // Type of top value is equal to type ID (TYPE(IS ...)) (reads: typeid; pushes: result)
    PB_OP_TYPEAPEQ,     // Type of top value is approximately equal to type ID (TYPE(SIMILAR ...)) (reads: typeid; pushes: result)
    PB_OP_COLLNEXT,     // Gets the next object in a collection (COLL(NEXT)) (pops: obj; pushes: argobj)
    PB_OP_COLLSKIP,     // Go forwards by the specified amount (COLL(SKIP ...)) (pops: amount, obj)
    PB_OP_COLLBACK,     // Go back by the specified amount (COLL(BACK ...)) (pops: amount, obj)
    PB_OP_COLLTELL,     // Gets the index of the current arg (COLL(TELL)) (pops: obj; pushes: index)
    PB_OP_COLLSEEK,     // Go to the specified arg (COLL(SEEK)) (pops: index, obj)
    PB_OP_COLLINS,      // Inserts an object at the end (COLL(INS ...)) (pops: obj, obj)
    PB_OP_COLLINSI,     // Inserts an object at the specified index (COLL(INS ..., ...)) (pops: obj, index, obj)
    PB_OP_COLLDEL,      // Removes the specified arg (COLL(DEL ...)) (pops: index, obj)
    PB_OP_COLLREPL,     // Replaces the specified arg (COLL(REPL ...)) (pops: obj, index, obj)
    PB_OP_EVLSNSYNC,    // Adds an event listener for a SYNC event (EVENT(LISTEN SYNC "..." SUB ...)) (pops: sub, event; pushes: id)
    PB_OP_EVLSNSYNCI,   // Adds an event listener from the stack for a SYNC event (EVENT(LISTEN SYNC "..." SUBH ...)) (pops: sub, event; pushes: id)
    PB_OP_EVLSNASYNC,   // Adds an event listener for an ASYNC event (EVENT(LISTEN ASYNC "..." SUB ...)) (pops: sub, event; pushes: id)
    PB_OP_EVLSNASYNCI,  // Adds an event listener from the stack for an ASYNC event (EVENT(LISTEN ASYNC "..." SUBH ...)) (pops: sub, event; pushes: id)
    PB_OP_EVIGNSYNC,    // Removes an event listener from a SYNC event (EVENT IGNORE SYNC ...) (pops: id, event)
    PB_OP_EVIGNASYNC,   // Removes an event listener from an ASYNC event (EVENT IGNORE ASYNC ...) (pops: id, event)
    PB_OP_EVFIRESYNC,   // Fires a SYNC event (EVENT(FIRE SYNC ...)) (pops: args..., framemrkr, event; pushes: retval)
    PB_OP_EVFIREASYNC,  // Fires an ASYNC event (EVENT FIRE ASYNC ...) (pops: args..., framemrkr, event)
    PB_OP_SLEEP,        // Sleep (SLEEP ...) (pops: seconds (f32/f64) or microseconds)
    PB_OP_SLEEPINF,     // Sleep (SLEEP)
    PB_OP_SETLINE,      // Set the current line for debugging and error info (reads: line)
    PB_OP_SETCOL,       // Set the current column for debugging and error info (reads: col)
    PB_OP_SETNAME       // Set the current file/arena name for debugging and error info (reads: index)
};

enum pb_type {
    PB_TYPE_ANY = -1,
    PB_TYPE_VOID,
    PB_TYPE_NULL,
    PB_TYPE_STR,
    PB_TYPE_BOOL,
    PB_TYPE_I8,
    PB_TYPE_I16,
    PB_TYPE_I32,
    PB_TYPE_I64,
    PB_TYPE_U8,
    PB_TYPE_U16,
    PB_TYPE_U32,
    PB_TYPE_U64,
    PB_TYPE_F32,
    PB_TYPE_F64,
    PB_TYPE_COLL,
    PB_TYPE_SUB,
    PB_TYPE__COUNT
};

PACKEDENUM pb_typedef_type {
    PB_TYPEDEF_TYPE_ALIAS,
    PB_TYPEDEF_TYPE_COMPLEX,
    PB_TYPEDEF_TYPE_REF,
    PB_TYPEDEF_TYPE_CONST,
    PB_TYPEDEF_TYPE_NULLABLE
};
struct pb_typedef_complex {
    uint32_t membct;
    uint32_t* membids;
};
struct pb_typedef {
    enum pb_typedef_type type;
    union {
        uint32_t alias;
        struct pb_typedef_complex* complex;
        uint32_t ref;
        uint32_t constant;
        uint32_t nullable;
    };
};

struct pb_memb {
    const char* name;
    uint32_t namecrc;
};

struct pb_rodata_str {
    const char* data;
    uint32_t len;
};
struct pb_rodata_coll {
    struct pb_rodata* data;
    uint32_t len;
};
struct pb_rodata_complex {
    struct pb_rodata* membs;
    uint32_t membct;
};
struct pb_rodata {
    uint32_t type;
    uint32_t dims;
    union {
        uint32_t len;
        const uint32_t* lens;
    };
    union {
        struct pb_rodata_str str;
        bool b; // would be named 'bool'
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
        struct pb_rodata_coll coll;
        uint32_t sub;
        struct pb_rodata_complex complex;
        union {
            struct pb_rodata_str* str;
            uint8_t* b;
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
            struct pb_rodata_coll* coll;
            uint32_t* sub;
            struct pb_rodata_complex* complex;
        } array;
    };
};

PACKEDENUM pb_obj_type {
    PB_OBJ_TYPE_CONST,
    PB_OBJ_TYPE_DATA,
    PB_OBJ_TYPE_INDEX,
    PB_OBJ_TYPE_ARGC,
    PB_OBJ_TYPE_ARGV,
    PB_OBJ_TYPE_SUB,
    PB_OBJ_TYPE_EXTERNAL
};
struct pb_obj_data_coll {
    uint32_t* objs;
    uint32_t len;
    uint32_t pos;
};
struct pb_obj_data_complex {
    uint32_t* membs;
    uint32_t membct;
};
struct pb_obj_data {
    uint32_t type;
    uint32_t dims;
    union {
        uint32_t len;
        const uint32_t* lens;
    };
    union {
        struct charbuf str;
        bool b;
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
        struct pb_obj_data_coll coll;
        uint32_t sub;
        struct pb_obj_data_complex complex;
        union {
            struct charbuf* str;
            uint8_t* b;
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
            struct pb_obj_data_coll* coll;
            uint32_t* sub;
            struct pb_obj_data_complex* complex;
        } array;
    };
};
struct pb_obj {
    enum pb_obj_type type;
    union {
        struct pb_rodata* constant; // would be named 'const'
        struct pb_obj_data* data;
        struct {
            uint32_t obj;
            uint32_t dims;
            union {
                uint32_t index;
                struct VLB(uint32_t) inds;
            };
        } index;
        struct {
            uint32_t index;
        } argv;
        struct {
            uint32_t id;
        } sub;
        struct {
            uint32_t procid;
            uint32_t obj;
        } external;
    };
};

PACKEDENUM pb_stackelem_type {
    PB_STACKELEM_TYPE_DUP,
    PB_STACKELEM_TYPE_OBJ,
    PB_STACKELEM_TYPE_OBJCAT,
    PB_STACKELEM_TYPE_FRAME
};
struct pb_stackelem {
    enum pb_stackelem_type type;
    union {
        uint32_t obj;
        struct {
            uint32_t lobj;
            uint32_t robj;
        } objcat;
    };
};

struct pb_prog_sub {
    const uint32_t* bytecode;
    uint32_t argct;
    union {
        uint32_t argtype;
        const uint32_t* argtypes;
    };
    uint32_t rettype;
};
struct pb_prog {
    const uint32_t* bytecode;
    const void* constdata;
    struct pb_rodata* consts;
    uint32_t constct;
    uint32_t gvarct;
    const char** filenames;
    uint32_t filenamect;
};

PACKEDENUM pb_proc_status_waitingon {
    PB_PROC_STATUS_WAITINGON_SLEEP,
    PB_PROC_STATUS_WAITINGON_CHILD,
    PB_PROC_STATUS_WAITINGON_EVENT
};
struct pb_proc_status {
    uint8_t running : 1;
    uint8_t finished : 1;
    uint8_t error : 1;
    uint8_t waiting : 1;
    enum pb_proc_status_waitingon waitingon;
};
struct pb_proc {
    struct pb_proc_status status;
    uint32_t progid;
    uint32_t argc;
    struct pb_rodata* argv;
    struct VLB(struct pb_obj) objs;
    struct VLB(uint32_t) gvars;
    struct VLB(uint32_t) lvars;
    struct VLB(struct pb_stackelem) stack;
};

struct pb_proc_exec_opt {
    uint32_t mininst;           // min amount of inst to run before considering return conditions
    uint32_t maxinst;           // max amount of inst to run before returning (0 for inf)
    unsigned retonsleep : 1;    // return on PB_OP_SLEEP
    unsigned retbeforesub : 1;  // return before PB_OP_JSR or PB_OP_JSRV
    unsigned retafterret : 1;   // return after PB_OP_RET or PB_OP_RETV
    unsigned retafterjmpf : 1;  // return after PB_OP_JMP forwards
    unsigned retafterjmpb : 1;  // return after PB_OP_JMP backwards
    unsigned retafterbf : 1;    // return after PB_OP_B forwards
    unsigned retafterbb : 1;    // return after PB_OP_B backwards
    unsigned retafterccall : 1; // return after PB_OP_CCALL
};
#define PB_PROC_EXEC_OPT_DEFAULTS {.mininst = 0, .maxinst = 256, .retonsleep = 1, .retafterret = 1, .retafterjmpb = 1, .retafterbb = 1, .retafterccall = 1}

typedef enum pb_error (*pb_proc_ccallcb)(struct pbasic*, uint32_t procid, const void* userdata);

typedef enum pb_error (*pb_event_synchandler)(struct pbasic*, uint32_t procid, const char* name, uint32_t namelen, void* userdata);
typedef enum pb_error (*pb_event_asynchandler)(struct pbasic*, uint32_t procid, const char* name, uint32_t namelen, void* userdata);

PACKEDENUM pb_preproc_type {
    PB_PREPROC_TYPE_VOID,
    PB_PREPROC_TYPE_STR,
    PB_PREPROC_TYPE_BOOL,
    PB_PREPROC_TYPE_I8,
    PB_PREPROC_TYPE_I16,
    PB_PREPROC_TYPE_I32,
    PB_PREPROC_TYPE_I64,
    PB_PREPROC_TYPE_U8,
    PB_PREPROC_TYPE_U16,
    PB_PREPROC_TYPE_U32,
    PB_PREPROC_TYPE_U64,
    PB_PREPROC_TYPE_F32,
    PB_PREPROC_TYPE_F64
};
struct pb_preproc_data {
    enum pb_preproc_type type;
    union {
        struct {
            const char* data;
            size_t len;
        } str;
        bool b;
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
    };
};
struct pb_preproc_var {
    size_t name;
    uint32_t namecrc;
    struct pb_preproc_data data;
};

PACKEDENUM pb_compiler_opt_olvl {
    PB_COMPILER_OPT_OLVL_0,
    PB_COMPILER_OPT_OLVL_1,
    PB_COMPILER_OPT_OLVL_DEFAULT = 1,
    PB_COMPILER_OPT_OLVL_MIN = 0,
    PB_COMPILER_OPT_OLVL_MAX = 1
};
PACKEDENUM pb_compiler_opt_dbglvl {
    PB_COMPILER_OPT_DBGLVL_OFF,
    PB_COMPILER_OPT_DBGLVL_BASIC,
    PB_COMPILER_OPT_DBGLVL_FULL,
    PB_COMPILER_OPT_DBGLVL_DEFAULT = 1,
    PB_COMPILER_OPT_DBGLVL_MIN = 0,
    PB_COMPILER_OPT_DBGLVL_MAX = 2
};
struct pb_compiler_opt_preprocvar {
    const char* name;
    uint32_t namecrc;
    struct pb_preproc_data data;
};

typedef enum pb_error (*pb_compiler_addon_cb_evalcmd)( // also rets enum pb_compitf_evalexprret
    struct pb_compiler*, const char* ns, uint32_t nscrc, const char* name, uint32_t namecrc,
    struct pb_compiler_tok* tcdata, size_t tclen, struct charbuf* tcstr,
    bool func, unsigned depth, unsigned evalflags, struct pb_rodata* d
);
typedef enum pb_error (*pb_compiler_addon_cb_evalpreproccmd)(
    struct pb_compiler*, const char* ns, uint32_t nscrc, const char* name, uint32_t namecrc,
    struct pb_compiler_tok* tcdata, size_t tclen, struct charbuf* tcstr,
    bool func, unsigned depth, struct pb_preproc_data* d
);
typedef enum pb_error (*pb_compiler_addon_cb_evalvar)( // also rets enum pb_compitf_evalexprret
    struct pb_compiler*, const char* ns, uint32_t nscrc, const char* name, uint32_t namecrc,
    unsigned evalflags, struct pb_rodata* d
);
typedef enum pb_error (*pb_compiler_addon_cb_evalpreprocvar)(
    struct pb_compiler*, const char* ns, uint32_t nscrc, const char* name, uint32_t namecrc,
    struct pb_preproc_data* d
);
struct pb_compiler_addon {
    pb_compiler_addon_cb_evalpreproccmd preproccmd;
    pb_compiler_addon_cb_evalpreprocvar preprocvar;
    pb_compiler_addon_cb_evalcmd cmd;
    pb_compiler_addon_cb_evalvar var;
};

struct pb_compiler_opt {
    enum pb_compiler_opt_olvl olvl;
    enum pb_compiler_opt_dbglvl dbglvl;
    const struct pb_compiler_opt_preprocvar* preprocvars;
    size_t preprocvarct;
    const struct pb_compiler_addon* addons;
    size_t addonct;
    const char* errprefix;
};
#define PB_COMPILER_OPT_DEFAULTS {.olvl = PB_COMPILER_OPT_OLVL_DEFAULT, .dbglvl = PB_COMPILER_OPT_DBGLVL_DEFAULT}
struct pb_compiler_srcloc {
    size_t src;
    unsigned long line;
    unsigned long col;
};
PACKEDENUM pb_compiler_tok_type {
    PB_COMPILER_TOK_TYPE_SYM,
    PB_COMPILER_TOK_TYPE_ID,
    PB_COMPILER_TOK_TYPE_DATA
};
PACKEDENUM pb_compiler_tok_subtype {
    PB_COMPILER_TOK_SUBTYPE_SYM_EXCL,
    PB_COMPILER_TOK_SUBTYPE_SYM_EXCLEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_DOLLAR,
    PB_COMPILER_TOK_SUBTYPE_SYM_PERC,
    PB_COMPILER_TOK_SUBTYPE_SYM_PERCEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_AMP,
    PB_COMPILER_TOK_SUBTYPE_SYM_AMPAMP,
    PB_COMPILER_TOK_SUBTYPE_SYM_AMPEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_OPENPAREN,
    PB_COMPILER_TOK_SUBTYPE_SYM_CLOSEPAREN,
    PB_COMPILER_TOK_SUBTYPE_SYM_AST,
    PB_COMPILER_TOK_SUBTYPE_SYM_ASTEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_PLUS,
    PB_COMPILER_TOK_SUBTYPE_SYM_PLUSEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_COMMA,
    PB_COMPILER_TOK_SUBTYPE_SYM_DASH,
    PB_COMPILER_TOK_SUBTYPE_SYM_DASHEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_DOT,
    PB_COMPILER_TOK_SUBTYPE_SYM_DOTDOTDOT,
    PB_COMPILER_TOK_SUBTYPE_SYM_SLASH,
    PB_COMPILER_TOK_SUBTYPE_SYM_COLON,
    PB_COMPILER_TOK_SUBTYPE_SYM_LT,
    PB_COMPILER_TOK_SUBTYPE_SYM_LTLT,
    PB_COMPILER_TOK_SUBTYPE_SYM_LTEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_LTGT,
    PB_COMPILER_TOK_SUBTYPE_SYM_LTLTLT,
    PB_COMPILER_TOK_SUBTYPE_SYM_LTLTEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_LTLTLTEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_EQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_EQEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_GT,
    PB_COMPILER_TOK_SUBTYPE_SYM_GTGT,
    PB_COMPILER_TOK_SUBTYPE_SYM_GTEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_GTGTGT,
    PB_COMPILER_TOK_SUBTYPE_SYM_GTGTEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_GTGTGTEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_QUESTION,
    PB_COMPILER_TOK_SUBTYPE_SYM_AT,
    PB_COMPILER_TOK_SUBTYPE_SYM_OPENSQB,
    PB_COMPILER_TOK_SUBTYPE_SYM_CLOSESQB,
    PB_COMPILER_TOK_SUBTYPE_SYM_CARET,
    PB_COMPILER_TOK_SUBTYPE_SYM_CARETEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_OPENCURB,
    PB_COMPILER_TOK_SUBTYPE_SYM_BAR,
    PB_COMPILER_TOK_SUBTYPE_SYM_BARBAR,
    PB_COMPILER_TOK_SUBTYPE_SYM_BAREQ,
    PB_COMPILER_TOK_SUBTYPE_SYM_CLOSECURB,
    PB_COMPILER_TOK_SUBTYPE_SYM_TILDE,
    PB_COMPILER_TOK_SUBTYPE_SYM_TILDEEQ,
    PB_COMPILER_TOK_SUBTYPE_SYM__COUNT,
    PB_COMPILER_TOK_SUBTYPE_DATA_STR = 0,
    PB_COMPILER_TOK_SUBTYPE_DATA_BOOL,
    PB_COMPILER_TOK_SUBTYPE_DATA_I8,
    PB_COMPILER_TOK_SUBTYPE_DATA_I16,
    PB_COMPILER_TOK_SUBTYPE_DATA_I32,
    PB_COMPILER_TOK_SUBTYPE_DATA_I64,
    PB_COMPILER_TOK_SUBTYPE_DATA_U8,
    PB_COMPILER_TOK_SUBTYPE_DATA_U16,
    PB_COMPILER_TOK_SUBTYPE_DATA_U32,
    PB_COMPILER_TOK_SUBTYPE_DATA_U64,
    PB_COMPILER_TOK_SUBTYPE_DATA_F32,
    PB_COMPILER_TOK_SUBTYPE_DATA_F64,
    PB_COMPILER_TOK_SUBTYPE_DATA__COUNT
};
struct pb_compiler_tok {
    enum pb_compiler_tok_type type;
    enum pb_compiler_tok_subtype subtype;
    struct pb_compiler_srcloc loc;
    union {
        size_t id;
        union {
            struct {
                size_t off;
                size_t len;
            } str;
            bool b;
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
        } data;
    };
};
struct pb_compiler_tokcoll {
    struct pb_compiler_tok* data;
    size_t len;
    size_t size;
    struct charbuf strings;
    struct VLB(size_t) brackets;
    char preprocinsstage;
    struct pb_compiler_srcloc preprocinsloc;
    struct pb_compiler_srcloc preprocinsidloc;
    size_t preprocinsidpos;
};
struct pb_compiler_source {
    size_t name;
    const char* type;
    struct pb_compiler_srcloc from;
};
struct pb_compiler_stream {
    struct datastream* ds;
    unsigned long line;
    unsigned long col;
    unsigned long oldcol;
    size_t src;
};
struct pb_compiler_label {
    char* name;
    uint32_t namecrc;
    uint32_t inst;
};
struct pb_compiler_irelem {
    uint32_t next;
    uint32_t pos;
};
struct pb_compiler_progir {
    struct VLB(uint32_t) data;
    struct VLB(struct pb_compiler_irelem) elems;
    struct VLB(struct pb_compiler_irlabel) labels;
    struct VLB(struct pb_rodata) consts;
};
PACKEDENUM pb_compiler_scopeid_type {
    PB_COMPILER_SCOPEID_TYPE_EXPR,
    PB_COMPILER_SCOPEID_TYPE_LABEL,
    PB_COMPILER_SCOPEID_TYPE_TYPEID
};
PACKEDENUM pb_compiler_scopeid_subtype {
    PB_COMPILER_SCOPEID_SUBTYPE_EXPR_VAR,
    PB_COMPILER_SCOPEID_SUBTYPE_EXPR_CONST,
    PB_COMPILER_SCOPEID_SUBTYPE_EXPR_ARG,
    PB_COMPILER_SCOPEID_SUBTYPE_EXPR_SUB
};
struct pb_compiler_scopeid {
    enum pb_compiler_scopeid_type type;
    enum pb_compiler_scopeid_subtype subtype;
    size_t fullnameoff;
    size_t nameoff;
    size_t ns;
    union {
        union {
            uint32_t var;
            uint32_t constant;
            uint32_t sub;
            uint32_t arg;
        } expr;
        uint32_t label;
        uint32_t typeid;
    };
};
PACKEDENUM pb_compiler_scope_type {
    PB_COMPILER_SCOPE_TYPE_BLOCK,
    PB_COMPILER_SCOPE_TYPE_NS,
    PB_COMPILER_SCOPE_TYPE_SUB,
    PB_COMPILER_SCOPE_TYPE_IF,
    PB_COMPILER_SCOPE_TYPE_ELIF,
    PB_COMPILER_SCOPE_TYPE_ELSE,
    PB_COMPILER_SCOPE_TYPE_WHILE,
    PB_COMPILER_SCOPE_TYPE_DO,
    PB_COMPILER_SCOPE_TYPE_SWITCH,
    PB_COMPILER_SCOPE_TYPE_CASE,
};
struct pb_compiler_scope {
    enum pb_compiler_scope_type type;
    struct VLB(size_t) using;
    union {
        struct {
            struct VLB(struct pb_compiler_scopeid) ids;
            size_t varct;
            size_t stroff;
        };
        size_t ns;
    };
};
struct pb_compiler_ns {
    size_t fullnameoff;
    size_t nameoff;
    uint32_t fullnamecrc;
    uint32_t namecrc;
};
struct pb_compiler_sub {
    struct pb_compiler_progir progir;
    uint32_t argct;
    union {
        uint32_t argtype;
        const uint32_t* argtypes;
    };
    uint32_t rettype;
};
struct pb_compiler {
    struct pbasic* pb;
    const struct pb_compiler_opt* opt;
    uint32_t progid;
    unsigned preproccond : 1;
    struct pb_compiler_stream stream;
    struct VLB(struct pb_compiler_stream) prevstreams;
    struct {
        struct pb_compiler_source* data;
        size_t len;
        size_t size;
        struct charbuf names;
    } sources;
    struct {
        struct pb_preproc_var* data;
        size_t len;
        size_t size;
        struct charbuf names;
    } preprocvars;
    struct pb_compiler_tokcoll comptok;
    struct pb_compiler_tokcoll preproctok;
    struct {
        struct pb_compiler_scope* data;
        size_t len;
        size_t size;
        struct charbuf strings;
    } scopes;
    struct {
        struct pb_compiler_ns* data;
        size_t len;
        size_t size;
        struct charbuf strings;
    } namespaces;
    size_t curns;
    struct pb_compiler_progir progir;
    struct VLB(struct pb_compiler_sub) subs;
    size_t cursub;
    struct charbuf* err;
};

enum pb_compitf_evalexprret {
    PB_COMPITF_EVALEXPRRET_RUNTIME = -1,
    PB_COMPITF_EVALEXPRRET_COMPILETIME = -2,
};
#define PB_COMPITF_EVALEXPRFLAG_FORCERUNTIME     (1U << 0)
#define PB_COMPITF_EVALEXPRFLAG_FORCECOMPILETIME (1U << 1)
#define PB_COMPITF_EVALEXPRFLAG_ALLOWCOLLDEFER   (1U << 2)
#define PB_COMPITF_EVALEXPRFLAG_NOOUTPUT         (1U << 3)

struct pbasic {
    struct {
        struct pb_typedef* data;
        size_t len;
        size_t size;
        #if PSRC_MTLVL >= 2
        struct accesslock lock;
        #endif
    } typedefs;
    struct {
        struct pb_memb* data;
        size_t len;
        size_t size;
        #if PSRC_MTLVL >= 2
        struct accesslock lock;
        #endif
    } membs;
    struct {
        struct pb_prog* data;
        size_t len;
        size_t size;
        #if PSRC_MTLVL >= 2
        struct accesslock lock;
        #endif
    } progs;
    struct {
        struct pb_proc* data;
        size_t len;
        size_t size;
        #if PSRC_MTLVL >= 2
        struct accesslock lock;
        #endif
    } procs;
    struct {
        struct VLB(struct pb_event_sync) sync;
        struct VLB(struct pb_event_async) async;
        #if PSRC_MTLVL >= 2
        struct accesslock lock;
        #endif
    } events;
    const pb_proc_ccallcb* ccalltable;
};

struct pb_create_opt {
    const pb_proc_ccallcb* ccalltable;
};

enum pb_error pb_create(struct pbasic*, const struct pb_create_opt*);
void pb_destroy(struct pbasic*);

enum pb_error pb_prog_compile(struct pbasic*, struct datastream*, const char* type, const struct pb_compiler_opt*, uint32_t* progidout, struct charbuf* err);
void pb_prog_destroy(struct pbasic*, uint32_t progid);

void pb_compitf_puterr(struct pb_compiler*, enum pb_error e, const char* msg, const struct pb_compiler_srcloc*);
bool pb_compitf_pushsource(struct pb_compiler*, struct datastream*, const char* type, const struct pb_compiler_srcloc*);
enum pb_error pb_compitf_evalexpr(struct pb_compiler*, struct pb_compiler_tok* tcdata, size_t tclen, struct charbuf* tcstr, unsigned flags, unsigned depth, size_t* ctout, struct pb_rodata*);
enum pb_error pb_compitf_evalpreprocexpr(struct pb_compiler*, struct pb_compiler_tok* tcdata, size_t tclen, struct charbuf* tcstr, unsigned depth, size_t* ctout, struct pb_preproc_data*);

enum pb_error pb_proc_create(struct pbasic*, uint32_t parent, uint32_t progid, uint32_t argc, struct pb_rodata* argv, uint32_t* procidout);
void pb_proc_destroy(struct pbasic*, uint32_t procid);
enum pb_error pb_proc_reset(struct pbasic*, uint32_t procid, uint32_t progid, uint32_t argc, struct pb_rodata* argv);
enum pb_error pb_proc_exec(struct pbasic*, uint32_t procid, struct pb_proc_exec_opt*, struct charbuf* err);
void pb_proc_kill(struct pbasic*, uint32_t procid);

enum pb_error pb_event_firesync(struct pbasic*, const char* name, uint32_t namelen, uint32_t argc, struct pb_rodata* argv, struct pb_rodata* ret);
uint32_t pb_event_setsynchandler(struct pbasic*, const char* name, uint32_t namelen, pb_event_synchandler, void* userdata);
bool pb_event_delsynchandler(struct pbasic*, const char* name, uint32_t namelen, uint32_t id);
enum pb_error pb_event_fireasync(struct pbasic*, const char* name, uint32_t namelen, uint32_t argc, struct pb_rodata* argv);
uint32_t pb_event_setasynchandler(struct pbasic*, const char* name, uint32_t namelen, pb_event_asynchandler, void* userdata);
bool pb_event_delasynchandler(struct pbasic*, const char* name, uint32_t namelen, uint32_t id);

void pb_rodata_destroy(struct pbasic*, struct pb_rodata*);

static ALWAYSINLINE void pb_proc_getstatus(struct pbasic* pb, uint32_t procid, struct pb_proc_status* status) {
    #if PSRC_MTLVL >= 2
    acquireReadAccess(&pb->procs.lock);
    #endif
    *status = pb->procs.data[procid].status;
    #if PSRC_MTLVL >= 2
    releaseReadAccess(&pb->procs.lock);
    #endif
}

static inline void pb_compitf_mksrcloc(struct pb_compiler* pbc, int prevchar, struct pb_compiler_srcloc* l) {
    l->src = pbc->stream.src;
    if (prevchar == -1) {
        l->line = pbc->stream.line;
        l->col = pbc->stream.col;
    } else {
        if (prevchar != '\n') {
            l->line = pbc->stream.line;
            l->col = pbc->stream.col - 1;
        } else {
            l->line = pbc->stream.line - 1;
            l->col = pbc->stream.oldcol;
        }
    }
}
static ALWAYSINLINE void pb_compitf_puterrln(struct pb_compiler* pbc, enum pb_error e, const char* msg, struct pb_compiler_srcloc* el) {
    pb_compitf_puterr(pbc, e, msg, el);
    cb_add(pbc->err, '\n');
}

extern const char* pb__error_str[PB_ERROR__COUNT];
static ALWAYSINLINE const char* pb_strerror(enum pb_error e) {
    return pb__error_str[e];
}

static inline bool pb_util_checkdims(uint32_t dims, const uint32_t* sizes) {
    if (dims < 2) return true;
    uint32_t sz1 = sizes[0];
    uint32_t sz2 = sizes[1];
    uint32_t tmp1 = (sz1 >> 16) * (sz2 & 0xFFFF);
    uint32_t tmp2 = (sz1 & 0xFFFF) * (sz2 >> 16);
    for (uint32_t i = 2; i < dims; ++i) {
        if ((sz1 >> 16) * (sz2 >> 16) + (tmp1 >> 16) + (tmp2 >> 16)) return false;
        sz1 *= sz2;
        sz2 = sizes[i];
        tmp1 = (sz1 >> 16) * (sz2 & 0xFFFF);
        tmp2 = (sz1 & 0xFFFF) * (sz2 >> 16);
    }
    return !((sz1 >> 16) * (sz2 >> 16) + (tmp1 >> 16) + (tmp2 >> 16));
}
void pb_util_compiler_opt_createclone(const struct pb_compiler_opt*, struct pb_compiler_opt*);
void pb_util_compiler_opt_destroyclone(struct pb_compiler_opt*);
bool pb_util_compiler_opt_cmp(const struct pb_compiler_opt*, const struct pb_compiler_opt*);

static ALWAYSINLINE int pb__compiler_getc_inline(struct pb_compiler* pbc) {
    again:;
    int c = ds_text_getc_fullinline(pbc->stream.ds);
    if (c != DS_END && c) {
        if (c != '\n') {
            ++pbc->stream.col;
        } else {
            ++pbc->stream.line;
            pbc->stream.oldcol = pbc->stream.col;
            pbc->stream.col = 1;
        }
        return c;
    }
    if (!pbc->prevstreams.len) return -1;
    ds_close(pbc->stream.ds);
    pbc->stream = pbc->prevstreams.data[--pbc->prevstreams.len];
    goto again;
}
int pb__tokenize(struct pb_compiler* pbc, struct pb_compiler_tokcoll* tc, bool preproc, int* cptr, enum pb_error* e);
enum pb_error pb__evalpreproccmd(struct pb_compiler* pbc, char* id, struct pb_compiler_tok* tcdata, size_t tclen, struct charbuf* tcstr, bool func, unsigned depth, struct pb_preproc_data* d);

#endif
