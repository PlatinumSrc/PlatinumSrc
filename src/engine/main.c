#include "renderer.h"
#include <logging.h>
#include <version.h>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    plog(LOG_PLAIN, "PlatinumSrc build %u", (unsigned)PSRC_BUILD);
    return 0;
}
