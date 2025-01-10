#include <stdio.h>

const char* pfatool_extract_help =
   "<INPUT> <OUTPUT> [PATH]... - Extract all or a list of PATHs in the INPUT archive to the OUTPUT path"
;

int pfatool_extract(char* argv0, int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "%s: Missing 'INPUT' param for 'extract' command\n", argv0);
        return 1;
    }
    if (argc < 2) {
        fprintf(stderr, "%s: Missing 'OUTPUT' param for 'extract' command\n", argv0);
        return 1;
    }
    return 0;
}
