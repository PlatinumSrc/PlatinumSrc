#include "common.h"

#include <stddef.h>

int quitreq = 0;

struct options options = {0};

char* maindir = NULL;
char* userdir = NULL;
char* gamedir = NULL;
char* savedir = NULL;

struct cfg* config = NULL;
struct cfg* gameconfig = NULL;

struct rc_script* mainscript;
