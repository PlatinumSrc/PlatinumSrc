#include <stdio.h>

const char* pfatool_create_help =
   "<OUTPUT> [PATH]... - Create an archive out of the provided PATHs"
;

int pfatool_create(char* argv0, int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "%s: Missing 'OUTPUT' param for 'create' command\n", argv0);
        return 1;
    }
    return 0;
}
