#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <string.h>

int main(void) {
    const char *path = "../../output_files/Elixir.FirstModule.beam";
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    // Skip BEAM header: "FOR1" + size + "BEAM"
    fseek(f, 12, SEEK_SET);

    char chunk_id[5] = {0};
    uint32_t raw_size, chunk_size;

    while (fread(chunk_id, 1, 4, f) == 4) {
        chunk_id[4] = '\0';

        if (fread(&raw_size, 1, 4, f) != 4) break;
        chunk_size = ntohl(raw_size);

        unsigned char *data = malloc(chunk_size);
        if (!data) {
            fprintf(stderr, "malloc failed\n");
            break;
        }

        if (fread(data, 1, chunk_size, f) != chunk_size) {
            fprintf(stderr, "Unexpected EOF reading %s chunk\n", chunk_id);
            free(data);
            break;
        }

        // --- Only handle Atom chunk (AtU8)
        if (strncmp(chunk_id, "AtU8", 4) == 0 || strncmp(chunk_id, "Atom", 4) == 0) {
            uint32_t atom_count;
            memcpy(&atom_count, data, 4);
            atom_count = ntohl(atom_count);

            printf("→ Atom chunk (%" PRIu32 " atoms):\n", atom_count);
            printf("%s\n", data);
            size_t offset = 4;  // Start after atom_count
            for (uint32_t i = 0; i < atom_count && offset < chunk_size; i++) {
                uint8_t len = data[offset++];
                if (offset + len > chunk_size) break;

                printf("len: %u, data: %p, offset: %zu", len, (void*)data, offset);
                printf(":%.*s\n", len, data + offset);  // print atom text
            }
        }

        free(data);

        // align to 4-byte boundary
        long pos = ftell(f);
        if (pos % 4 != 0) fseek(f, 4 - (pos % 4), SEEK_CUR);
    }

    fclose(f);
    return 0;
}
