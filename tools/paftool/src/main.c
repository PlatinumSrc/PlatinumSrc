#include <string.h>
#include <stdio.h>

#include "create.h"
#include "extract.h"
#include "modify.h"
#include "info.h"

int main(int argc, char** argv) {
    if (argc < 2 || !strcmp(argv[1], "--help")) {
        printf("USAGE: %s COMMAND ...\n", argv[0]);
        putchar('\n');
        puts("COMMANDS:");
        printf(
            "    c, create %s\n"
            "    e, x, extract %s\n"
            "    i, info %s\n"
            "    m, modify %s\n",
            paftool_create_help,
            paftool_extract_help,
            paftool_info_help,
            paftool_modify_help
        );
        return 0;
    } else if (!strcmp(argv[1], "c") || !strcmp(argv[1], "create")) {
        return paftool_create(argv[0], argc - 2, argv + 2);
    } else if (!strcmp(argv[1], "e") || !strcmp(argv[1], "x") || !strcmp(argv[1], "extract")) {
        return paftool_extract(argv[0], argc - 2, argv + 2);
    } else if (!strcmp(argv[1], "m") || !strcmp(argv[1], "modify")) {
        return paftool_modify(argv[0], argc - 2, argv + 2);
    } else if (!strcmp(argv[1], "i") || !strcmp(argv[1], "info")) {
        return paftool_info(argv[0], argc - 2, argv + 2);
    }
    fprintf(stderr, "%s: Unknown command '%s'\n", argv[0], argv[1]);
    return 1;
}
