#ifndef PSRC_COMMON_SCRIPTING
#define PSRC_COMMON_SCRIPTING

#include "string.h"
#include "threading.h"

#include <stdint.h>
#include <stdbool.h>

enum __attribute__((packed)) scriptopcode {
    SCRIPTOPCODE__INVAL = -1,
    SCRIPTOPCODE_TRUE, // set exit status to 0
    SCRIPTOPCODE_FALSE, // set exit status to 1
    SCRIPTOPCODE_JMP, // add to pc
    SCRIPTOPCODE_JMPTRUE, // set pc if exit status is 0
    SCRIPTOPCODE_JMPFALSE, // set pc if exit status is 1
    SCRIPTOPCODE_STATE, // push a new state
    SCRIPTOPCODE_RET, // pop to the previous state
    SCRIPTOPCODE_DEF, // define a subroutine
    SCRIPTOPCODE_EVDEF, // define an event subroutine
    SCRIPTOPCODE_ENDDEF, // end a subroutine definition
    SCRIPTOPCODE_SUB, // call a subroutine
    SCRIPTOPCODE_UNDEF, // unset a subroutine
    SCRIPTOPCODE_UNON, // unset an event subroutine
    SCRIPTOPCODE_ADD, // add data to the accumulator
    SCRIPTOPCODE_READVAR, // read a variable into the accumulator
    SCRIPTOPCODE_READVARSEP, // read a variable into the accumulator and push on separator characters
    SCRIPTOPCODE_READARRAY, // read array elements into the accumulator
    SCRIPTOPCODE_READARRAYSEP, // read array elements into the accumulator and push on separator characters
    SCRIPTOPCODE_PUSH, // flush the accumulator to a new out argument
    SCRIPTOPCODE_TEXT, // write text to the output
    SCRIPTOPCODE_SET, // set a variable
    SCRIPTOPCODE_GET, // get a variable
    SCRIPTOPCODE_SETARRAY, // set an array
    SCRIPTOPCODE_GETARRAY, // get an array
    SCRIPTOPCODE_UNSET, // delete a variable or array
    SCRIPTOPCODE_FIRE, // fire an event
    SCRIPTOPCODE_TEST, // compare
    SCRIPTOPCODE_MATH, // do math
    SCRIPTOPCODE_SLEEP, // delay execution
    SCRIPTOPCODE_CMD, // execute command
    SCRIPTOPCODE_EXIT, // terminate execution
    SCRIPTOPCODE__COUNT,
};
enum __attribute__((packed)) scriptopflag {
    SCRIPTOPFLAG_PIPE = 1 << 4, // put the output in the input of the next command
};

struct scriptstate;

typedef int (*scriptfunc_t)(struct scriptstate*, struct charbuf* i, int argc, struct charbuf* argv, struct charbuf* o);

struct __attribute__((packed)) scriptopdata_jmp {
    int pc;
};
struct __attribute__((packed)) scriptopdata_add {
    int offset;
    int len;
};
struct __attribute__((packed)) scriptopdata_readvar {
    int offset;
    int len;
    uint32_t crc;
};
struct __attribute__((packed)) scriptopdata_readarray {
    int offset;
    int len;
    uint32_t crc;
    int from;
    int to;
};
struct __attribute__((packed)) scriptopdata_cmd {
    scriptfunc_t func;
};

struct __attribute__((packed)) scriptop {
    enum scriptopcode opcode;
    union {
        struct scriptopdata_jmp jmp;
        struct scriptopdata_add add;
        struct scriptopdata_readvar readvar;
        struct scriptopdata_readarray readarray;
        struct scriptopdata_cmd cmd;
    };
};

struct script {
    char* strings;
    struct scriptop* ops;
};

struct scriptstatesub;
struct __attribute__((packed)) scriptstatedata {
    enum scriptopcode from;
    struct script script;
    struct scriptstatesub* sub;
    int pc;
    int loop;
    int ret;
    int argc;
    struct {
        char* data;
        int len;
    }* argv;
};
struct __attribute__((packed)) scriptstatevar {
    char* name;
    int namelen;
    uint32_t namecrc;
    int dim;
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
struct __attribute__((packed)) scriptstatesub {
    bool copied : 1;
    bool unset : 1;
    char* name;
    int namelen;
    uint32_t namecrc;
    struct script data;
    int uses;
};
struct __attribute__((packed)) scriptstateeventsub {
    struct scriptstatesub sub;
    int tableeventindex;
    int tablesubindex;
};
enum __attribute__((packed)) scriptwait {
    SCRIPTWAIT_NONE,
    SCRIPTWAIT_INTUNTIL, // positive value (interruptable by getting an event)
    SCRIPTWAIT_UNTIL, // negative value (uninterruptable)
    SCRIPTWAIT_INTINF, // sleep inf (interruptable by getting an event)
    SCRIPTWAIT_INF, // sleep -inf (uninterruptable, basically hangs the script)
    SCRIPTWAIT_EXIT,
};
struct scripteventtable;
struct scriptstate {
    #ifndef PSRC_NOMT
    mutex_t lock;
    #endif
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
        struct scriptstatesub* data;
        int len;
        int size;
    } subs;
    struct {
        struct scriptstateeventsub* data;
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
    int index;
};
struct scriptevent {
    char* name;
    uint32_t namecrc;
    struct scripteventsub* subs;
    int len;
    int size;
    int uses;
};
struct scripteventtable {
    #ifndef PSRC_NOMT
    struct accesslock lock;
    #endif
    struct scriptevent* data;
    int len;
    int size;
};

bool compileScript(char* path, scriptfunc_t (*findcmd)(char*), struct script* out, struct charbuf* e);
void cleanUpScript(struct script*);

bool createScriptEventTable(struct scripteventtable*, int);
void fireScriptEvent(struct scripteventtable*, char* name, int argc, struct charbuf* argv);
void destroyScriptEventTable(struct scripteventtable*);

struct scriptstate* newScriptState(struct script*, struct scripteventtable*, int argc, struct charbuf* argv);
bool execScriptState(struct scriptstate*, int*, struct charbuf*);
bool getScriptStateVar(struct scriptstate*, char* name, int namelen, struct scriptstatevar*);
void setScriptStateVar(struct scriptstate*, char* name, int namelen, int index, char* data, int datalen);
void resetScriptState(struct scriptstate*, struct script*); // start from beginning and change script if not NULL
void clearScriptState(struct scriptstate*, struct script*); // start from beginning, clear everything, and change script if not NULL
void deleteScriptState(struct scriptstate*);

#endif
