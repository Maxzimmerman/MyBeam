#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>
#include "load.h"

/* Main */
int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s file.beam\n", argv[0]);
        return 1;
    }

    int ok = load(argv);
    
    return ok ? 0 : 1;
}
