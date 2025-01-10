#include <stdio.h>

const char* paftool_info_help =
   "<INPUT> <COMMAND> ... - Display information about the INPUT archive\n"
   "        ls, list [OPTION]... [--] [PATH]... - List all or a list of PATHs\n"
   "            -e, --escape    - Escape special characters\n"
   "            -p, --plain     - Output just the path\n"
   "            -t, --tree      - Output a tree instead of a list\n"
   "        u, usage - Display usage statistics"
;

int paftool_info(char* argv0, int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "%s: Missing 'INPUT' param for 'info' command\n", argv0);
        return 1;
    }
    if (argc < 2) {
        fprintf(stderr, "%s: Missing 'COMMAND' param for 'info' command\n", argv0);
        return 1;
    }
    return 0;
}
