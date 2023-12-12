#ifndef PSRC_COMMON_SCRIPTING
#define PSRC_COMMON_SCRIPTING

#include "../utils/string.h"

#include <stdint.h>
#include <stdbool.h>

struct __attribute__((packed)) scriptstring {
    int offset;
    int len;
};

struct scriptstate;

typedef int (*scriptfunc_t)(struct scriptstate*, struct charbuf* i, int argc, struct charbuf* argv, struct charbuf* o);

enum __attribute__((packed)) scriptopcode {
    SCRIPTOPCODE_NOP, // no operation
    SCRIPTOPCODE_TRUE, // set exit status to 0
    SCRIPTOPCODE_FALSE, // set exit status to 1
    SCRIPTOPCODE_JMP, // jump
    SCRIPTOPCODE_JMPOK, // jump if return code == 0
    SCRIPTOPCODE_JMPFAIL, // jump if return code != 0
    SCRIPTOPCODE_JMPSUB, // start a sub-sub-state
    SCRIPTOPCODE_SET, // set/unset a variable
    SCRIPTOPCODE_ON, // subscribe to/unsubscribe from an event
    SCRIPTOPCODE_CMP, // compare
    SCRIPTOPCODE_EVAL, // evaluate a string as a command
    SCRIPTOPCODE_FUNC, // run a function
    SCRIPTOPCODE_RET, // return from a sub-state
    SCRIPTOPCODE_EXIT, // terminate execution
    SCRIPTOPCODE_CMD, // execute command
};

struct __attribute__((packed)) scriptopdata_jmp {
    int offset;
};
struct __attribute__((packed)) scriptopdata_jmpok {
    int offset;
};
struct __attribute__((packed)) scriptopdata_jmpfail {
    int offset;
};
struct __attribute__((packed)) scriptopdata_jmpsub {
    int offset;
};
struct __attribute__((packed)) scriptopdata_set {
    struct scriptstring eval;
};
struct __attribute__((packed)) scriptopdata_on {
    struct scriptstring eval;
};
struct __attribute__((packed)) scriptopdata_cmp {
    struct scriptstring eval;
};
struct __attribute__((packed)) scriptopdata_eval {
    struct scriptstring eval;
};
struct __attribute__((packed)) scriptopdata_func {
    struct scriptstring eval;
};
struct __attribute__((packed)) scriptopdata_call {
    struct scriptstring eval;
};
struct __attribute__((packed)) scriptopdata_ret {
    struct scriptstring eval;
};
struct __attribute__((packed)) scriptopdata_exit {
    struct scriptstring eval;
};
struct __attribute__((packed)) scriptopdata_cmd {
    scriptfunc_t func;
    struct scriptstring eval;
};

struct __attribute__((packed)) scriptop {
    enum scriptopcode opcode;
    union {
        struct scriptopdata_jmp jmp;
        struct scriptopdata_jmpok jmpok;
        struct scriptopdata_jmpfail jmpfail;
        struct scriptopdata_jmpsub jmpsub;
        struct scriptopdata_set set;
        struct scriptopdata_on on;
        struct scriptopdata_cmp cmp;
        struct scriptopdata_eval eval;
        struct scriptopdata_func func;
        struct scriptopdata_ret ret;
        struct scriptopdata_exit exit;
        struct scriptopdata_cmd cmd;
    };
};

struct script {
    char* strings;
    int opcount;
    struct scriptop* ops;
};

struct __attribute__((packed)) scriptstatedata {
    int pc;
    int ret;
    int argc;
    struct charbuf* argv;
};
struct __attribute__((packed)) scriptstatevar {
    char* name;
    int arraysize;
    union {
        struct {
            char* data;
            int len;
        } single;
        struct {
            char** data;
            int* len;
        } array;
    };
};
enum __attribute__((packed)) scriptwait {
    SCRIPTWAIT_NONE,
    SCRIPTWAIT_UNTIL,
    SCRIPTWAIT_INF,
};
struct scripteventtable;
struct scriptstate {
    struct script* script;
    struct scripteventtable* events;
    struct {
        struct scriptstatedata* data;
        int index;
        int size;
    } state;
    struct {
        struct scriptvardata* data;
        int len;
        int size;
    } vars;
    struct {
        struct {
            struct charbuf* data;
            int len;
            int size;
        } args;
        struct charbuf in;
        struct charbuf out;
        enum scriptwait waitstate;
        uint64_t waituntil;
    } tmp;
};

struct scripteventsub {
    struct scriptstate* script;
    int jump;
};
struct scriptevent {
    char* name;
    uint32_t namecrc;
    int uses;
    int subcount;
    struct scripteventsub* subs;
};
struct scripteventtable {
    struct scriptevent* data;
    int len;
    int size;
};

char* compileScript(char* path, scriptfunc_t (*findcmd)(char*), struct script* out);
void cleanUpScript(struct script*);

bool createScriptEventTable(struct scripteventtable*, int);
void fireScriptEvent(struct scripteventtable*, char* name, int argc, struct charbuf* argv);
void destroyScriptEventTable(struct scripteventtable*);

struct scriptstate* newScriptState(struct script*, struct scripteventtable*);
void resetScriptState(struct scriptstate*);
void deleteScriptState(struct scriptstate*);

#endif
