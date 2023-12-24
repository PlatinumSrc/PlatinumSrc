#ifndef PSRC_COMMON_SCRIPTING
#define PSRC_COMMON_SCRIPTING

#include "../utils/string.h"
#include "../utils/threading.h"

#include <stdint.h>
#include <stdbool.h>

struct __attribute__((packed)) scriptstring {
    int offset;
    int len;
};

struct scriptstate;

typedef int (*scriptfunc_t)(struct scriptstate*, struct charbuf* i, int argc, struct charbuf* argv, struct charbuf* o);

enum __attribute__((packed)) scriptopcode {
    SCRIPTOPCODE_TRUE, // set exit status to 0
    SCRIPTOPCODE_FALSE, // set exit status to 1
    SCRIPTOPCODE_JMP, // jump
    SCRIPTOPCODE_JMPOK, // jump if return code == 0
    SCRIPTOPCODE_JMPFAIL, // jump if return code != 0
    SCRIPTOPCODE_SUB, // start a sub-state
    SCRIPTOPCODE_FUNC, // call a funcion
    SCRIPTOPCODE_RET, // return from a sub-state
    SCRIPTOPCODE_SETFUNC, // set a function
    SCRIPTOPCODE_UNSETFUNC, // unset a function
    SCRIPTOPCODE_ENDFUNC, // marks the end of a function (no op)
    SCRIPTOPCODE_ON, // subscribe to an event
    SCRIPTOPCODE_UNON, // unsubscribe from an event
    SCRIPTOPCODE_ENDON, // marks the end of an event handler (no op)
    SCRIPTOPCODE_ADD, // add data to the accumulator
    SCRIPTOPCODE_PUSH, // flush the accumulator to a new out argument
    SCRIPTOPCODE_PIPE, // set the command input to the output of the last command 
    SCRIPTOPCODE_READVAR, // read a variable into the accumulator
    SCRIPTOPCODE_READVARSEP, // read a variable into the accumulator and push on separator characters
    SCRIPTOPCODE_READARG, // read an argument into the accumulator
    SCRIPTOPCODE_READARGSEP, // read an argument into the accumulator and push on separator characters
    SCRIPTOPCODE_READRET, // read the exit status into the accumulator
    SCRIPTOPCODE_READ, // returns data from and info about the provided input
    SCRIPTOPCODE_SET, // set a variable
    SCRIPTOPCODE_UNSET, // unset a variable
    SCRIPTOPCODE_GET, // get a variable
    SCRIPTOPCODE_CMP, // compare
    SCRIPTOPCODE_SLEEP, // delay execution
    SCRIPTOPCODE_CMD, // execute command
    SCRIPTOPCODE_EXIT, // terminate execution
};

struct __attribute__((packed)) scriptopdata_jmp {
    void* offset;
};
struct __attribute__((packed)) scriptopdata_jmpok {
    void* offset;
};
struct __attribute__((packed)) scriptopdata_jmpfail {
    void* offset;
};
struct __attribute__((packed)) scriptopdata_sub {
    void* offset;
};
struct __attribute__((packed)) scriptopdata_func {
    struct scriptstring name;
    uint32_t namecrc;
};
struct __attribute__((packed)) scriptopdata_setfunc {
    struct scriptstring name;
    uint32_t namecrc;
};
struct __attribute__((packed)) scriptopdata_unsetfunc {
    struct scriptstring name;
    uint32_t namecrc;
};
struct __attribute__((packed)) scriptopdata_on {
    struct scriptstring name;
    uint32_t namecrc;
};
struct __attribute__((packed)) scriptopdata_unon {
    struct scriptstring name;
    uint32_t namecrc;
};
struct __attribute__((packed)) scriptopdata_add {
    struct scriptstring data;
};
struct __attribute__((packed)) scriptopdata_readvar {
    struct scriptstring name;
    uint32_t namecrc;
};
struct __attribute__((packed)) scriptopdata_readvarsep {
    struct scriptstring name;
    uint32_t namecrc;
};
struct __attribute__((packed)) scriptopdata_readarg {
    int index;
};
struct __attribute__((packed)) scriptopdata_readargsep {
    int index;
};
struct __attribute__((packed)) scriptopdata_set {
    struct scriptstring name;
    uint32_t namecrc;
};
struct __attribute__((packed)) scriptopdata_unset {
    struct scriptstring name;
    uint32_t namecrc;
};
struct __attribute__((packed)) scriptopdata_get {
    struct scriptstring name;
    uint32_t namecrc;
};
struct __attribute__((packed)) scriptopdata_cmd {
    scriptfunc_t func;
};

struct __attribute__((packed)) scriptop {
    enum scriptopcode opcode;
    union {
        struct scriptopdata_jmp jmp;
        struct scriptopdata_jmpok jmpok;
        struct scriptopdata_jmpfail jmpfail;
        struct scriptopdata_sub sub;
        struct scriptopdata_func func;
        struct scriptopdata_setfunc setfunc;
        struct scriptopdata_unsetfunc unsetfunc;
        struct scriptopdata_on on;
        struct scriptopdata_unon unon;
        struct scriptopdata_add add;
        struct scriptopdata_readvar readvar;
        struct scriptopdata_readvarsep readvarsep;
        struct scriptopdata_readarg readarg;
        struct scriptopdata_readargsep readargsep;
        struct scriptopdata_set set;
        struct scriptopdata_unset unset;
        struct scriptopdata_get get;
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
    uint32_t namecrc;
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
struct __attribute__((packed)) scriptstatefunc {
    char* name;
    uint32_t namecrc;
    struct scriptop* data;
};
struct __attribute__((packed)) scriptstateeventsub {
    char* name;
    uint32_t namecrc;
    struct scriptop* data;
};
enum __attribute__((packed)) scriptwait {
    SCRIPTWAIT_NONE,
    SCRIPTWAIT_UNTIL,
    SCRIPTWAIT_INF,
};
struct scripteventtable;
struct scriptstate {
    mutex_t lock;
    struct script* script;
    struct scripteventtable* eventtable;
    struct {
        struct scriptstatedata* data;
        int index;
        int size;
    } state;
    struct {
        struct scriptstatevar* data;
        int len;
        int size;
    } vars;
    struct {
        struct scriptstatefunc* data;
        int len;
        int size;
    } funcs;
    struct {
        struct scriptestateeventsub* data;
        int len;
        int size;
    } eventsubs;
    struct {
        struct charbuf* data;
        int len;
        int size;
    } args;
    struct charbuf acc;
    struct charbuf in;
    struct charbuf out;
    enum scriptwait waitstate;
    uint64_t waituntil;
};

struct scripteventsub {
    struct scriptstate* script;
    int handler;
};
struct scriptevent {
    char* name;
    uint32_t namecrc;
    int uses;
    int subcount;
    struct scripteventsub* subs;
};
struct scripteventtable {
    struct accesslock lock;
    struct scriptevent* data;
    int len;
    int size;
};

bool compileScript(char* path, scriptfunc_t (*findcmd)(struct charbuf*), struct script* out, char** e);
void cleanUpScript(struct script*);

bool createScriptEventTable(struct scripteventtable*, int);
void fireScriptEvent(struct scripteventtable*, char* name, int argc, struct charbuf* argv);
void destroyScriptEventTable(struct scripteventtable*);

struct scriptstate* newScriptState(struct script*, struct scripteventtable*);
bool execScriptState(struct scriptstate*, int*);
void resetScriptState(struct scriptstate*, struct script*);
void clearScriptState(struct scriptstate*, struct script*);
void deleteScriptState(struct scriptstate*);

#endif
