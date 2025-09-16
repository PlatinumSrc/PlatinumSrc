#include "../rcmgralloc.h"
#include "pbasic.h"

const char* pb__error_str[PB_ERROR__COUNT] = {
    "No error",
    "Syntax error",
    "Type error",
    "Definition error",
    "Index error",
    "Value error",
    "Argument error",
    "Memory error",
    "Internal error"
};
