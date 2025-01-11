#include <stdio.h>

const char* paftool_modify_help =
   "<INPUT> <COMMAND> ... - Modify the INPUT archive\n"
   "        u, update <PAF_PATH> <FS_PATH> - Replace a file in the archive\n"
   "        cp, copy [OPTIONS] [--] <SOURCE>... <DEST> - Copy the SOURCE path(s) to the DEST path\n"
   "            -f, --force     - Overwrite\n"
   "            -r, --recursive - Copy directories and files recursively\n"
   "        mv, move [OPTIONS] [--] <SOURCE>... <DEST> - Move the SOURCE path(s) to the DEST path\n"
   "            -f, --force     - Overwrite\n"
   "        ln, link [OPTIONS] [--] <TARGET> <LINK> - Make a LINK pointing to TARGET\n"
   "            -f, --force     - Overwrite\n"
   "        rm, remove [OPTIONS] [--] <PATH>...\n"
   "            -r, --recursive - Remove directories and files recursively\n"
   "        o, optimize [OPTIONS] [--] <OUTPUT> - Create an optimized version of the INPUT archive\n"
   "            -s, --size      - Optimize for size (default)\n"
   "            -w, --writing   - Optimize for writing"
;

int paftool_modify(char* argv0, int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "%s: Missing 'INPUT' param for 'modify' command\n", argv0);
        return 1;
    }
    if (argc < 2) {
        fprintf(stderr, "%s: Missing 'COMMAND' param for 'modify' command\n", argv0);
        return 1;
    }
    return 0;
}
