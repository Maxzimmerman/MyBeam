#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>
#include "binary_parsing_helpers.h"

/* Main */
int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s file.beam\n", argv[0]);
        return 1;
    }

    byte *buf;
    usize size;
    printf("%s \n", buf);
    if (load_file(argv[1], &buf, &size) != 0) {
        printf("File load error\n");
        return 1;
    }

    int ok = walk_file(buf, size);
    free(buf);
    return ok ? 0 : 1;
}
