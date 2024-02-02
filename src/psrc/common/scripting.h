#ifndef PSRC_COMMON_SCRIPTING
#define PSRC_COMMON_SCRIPTING

#include "string.h"
#include "threading.h"

#include <stdint.h>
#include <stdbool.h>

struct scriptstate;

typedef int (*scriptfunc_t)(struct scriptstate*, struct charbuf* i, int argc, struct charbuf* argv, struct charbuf* o);

enum __attribute__((packed)) scriptopcode { // TODO: reorder to something less nonsensical
    SCRIPTOPCODE__INVAL = -1,
    SCRIPTOPCODE_TRUE, // set exit status to 0
    SCRIPTOPCODE_FALSE, // set exit status to 1
    SCRIPTOPCODE_AND, // run ops until END if the exit status is zero
    SCRIPTOPCODE_OR, // run ops until END if the exit status is non-zero
    SCRIPTOPCODE_IF, // start an IF block
    SCRIPTOPCODE_ELIF, // change to an ELIF block
    SCRIPTOPCODE_ELSE, // change to an ELSE block and run ops until END
    SCRIPTOPCODE_WHILE, // start a WHILE block
    SCRIPTOPCODE_CONT, // skip the rest of a WHILE block
    SCRIPTOPCODE_BREAK, // end a WHILE block early
    SCRIPTOPCODE_TESTBLOCK, // run conditions in an IF, ELIF, or WHILE block if the exit status is zero
    SCRIPTOPCODE_DEF, // set a subroutine
    SCRIPTOPCODE_ON, // subscribe to an event
    SCRIPTOPCODE_END, // end program flow blocks
    SCRIPTOPCODE_SUB, // call a subroutine
    SCRIPTOPCODE_RET, // return from a subroutine
    SCRIPTOPCODE_UNDEF, // unset a subroutine
    SCRIPTOPCODE_UNON, // unsubscribe from an event
    SCRIPTOPCODE_PIPE, // set the command input to the output of the last command 
    SCRIPTOPCODE_ADD, // add data to the accumulator
    SCRIPTOPCODE_PUSH, // flush the accumulator to a new out argument
    SCRIPTOPCODE_READ, // returns data from and info about the provided input
    SCRIPTOPCODE_READVAR, // read a variable into the accumulator
    SCRIPTOPCODE_READVARSEP, // read a variable into the accumulator and push on separator characters
    SCRIPTOPCODE_READARRAY, // read array elements into the accumulator
    SCRIPTOPCODE_READARRAYSEP, // read array elements into the accumulator and push on separator characters
    SCRIPTOPCODE_SET, // set a variable
    SCRIPTOPCODE_UNSET, // unset a variable
    SCRIPTOPCODE_GET, // get a variable
    SCRIPTOPCODE_TEST, // compare
    SCRIPTOPCODE_BRACTEST, // compare and the last arg must be ]
    SCRIPTOPCODE_TEXT, // write text to the output
    SCRIPTOPCODE_MATH, // do math
    SCRIPTOPCODE_FIRE, // fire an event
    SCRIPTOPCODE_SLEEP, // delay execution
    SCRIPTOPCODE_CMD, // execute command
    SCRIPTOPCODE_EXIT, // terminate execution
    SCRIPTOPCODE__COUNT,
};

struct __attribute__((packed)) scriptstring {
    int offset;
    int len;
};

struct __attribute__((packed)) scriptopdata_add {
    struct scriptstring data;
};
struct __attribute__((packed)) scriptopdata_readvar {
    struct scriptstring name;
    uint32_t namecrc;
};
struct __attribute__((packed)) scriptopdata_readarray {
    struct scriptstring name;
    uint32_t namecrc;
    int from;
    int to;
};
struct __attribute__((packed)) scriptopdata_cmd {
    scriptfunc_t func;
};

union scriptopdata {
    struct scriptopdata_add add;
    struct scriptopdata_readvar readvar;
    struct scriptopdata_readarray readarray;
    struct scriptopdata_cmd cmd;
};

struct __attribute__((packed)) scriptop {
    enum scriptopcode opcode;
    union scriptopdata data;
};

struct script {
    char* strings;
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
    bool valid : 1;
    bool copied : 1;
    char* name;
    uint32_t namecrc;
    struct scriptop* data;
};
struct __attribute__((packed)) scriptstateeventsub {
    bool valid : 1;
    bool copied : 1;
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

struct scriptstate* newScriptState(struct script*, struct scripteventtable*);
bool execScriptState(struct scriptstate*, int*);
void resetScriptState(struct scriptstate*, struct script*);
void clearScriptState(struct scriptstate*, struct script*);
void deleteScriptState(struct scriptstate*);

#endif
