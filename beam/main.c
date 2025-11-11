// beam_read_atoms.c
// A safe BEAM file chunk reader that prints all chunk IDs
// and fully decodes the AtU8/Atom chunk (UTF-8 atom names).

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>  // for ntohl()
#include <stdlib.h>
#include <inttypes.h>   // for PRIu32

int main(void) {
    const char *path = "../../output_files/Elixir.FirstModule.beam";
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    // --- Skip BEAM header: "FOR1" + file size + "BEAM"
    // BEAM files always start with:
    //   4 bytes: "FOR1"
    //   4 bytes: total file size
    //   4 bytes: "BEAM"

    // we allocate 5 bytes because c string need to end with a null terminator '\0'
    char header[5] = {0};
    uint32_t total_size;
    char  beam[5] = {0};
    fread(header, 1, 4, f);
    fread(&total_size, 1, 4, f);
    fread(beam, 1, 4, f);

    printf("|||||||||||||||FILE START|||||||||||||\n");
    printf("%s\n", header);
    printf("%u\n", total_size);
    printf("%s\n", beam);
    printf("|||||||||||||||FILE END|||||||||||||||\n");

    char chunk_id[5] = {0};
    uint32_t raw_size, chunk_size;

    // --- Loop through all chunks
    while (fread(chunk_id, 1, 4, f) == 4) {
        chunk_id[4] = '\0'; // NUL terminate for safe printing

        if (fread(&raw_size, 1, 4, f) != 4)
            break;

        chunk_size = ntohl(raw_size);
        printf("chunk id: %s, chunk size: %u bytes\n", chunk_id, chunk_size);

        unsigned char *data = malloc(chunk_size);
        if (!data) {
            fprintf(stderr, "malloc failed\n");
            break;
        }

        // --- Handle AtU8 or Atom chunks (atom tables)
        if (strncmp(chunk_id, "Atom", 4) == 0) {
            if (fread(data, 1, chunk_size, f) != chunk_size) {
                fprintf(stderr, "Unexpected EOF reading atom chunk\n");
                free(data);
                break;
            }

            // First 4 bytes = atom count (big-endian)
            uint32_t atom_count = ntohl(*(uint32_t *)data);
            printf("→ Atom chunk with %" PRIu32 " atoms\n", atom_count);

            size_t offset = 4; // skip count
            for (uint32_t i = 0; i < atom_count && offset < chunk_size; i++) {
                uint8_t len = data[offset++]; // read length
                if (offset + len > chunk_size) break; // safety check
                printf("  Atom %-3u: %.*s\n", i + 1, len, data + offset);
                offset += len;
            }

        } else if (strncmp(chunk_id, "Code", 4) == 0) {
            // --- Just skip over the code chunk for now
            fseek(f, chunk_size, SEEK_CUR);
            printf("→ Found Code chunk (skipped)\n");

        } else {
            // --- Other chunks (StrT, ImpT, ExpT, etc.)
            fseek(f, chunk_size, SEEK_CUR);
        }

        free(data);

        // --- Align to 4-byte boundary (BEAM chunks are padded)
        long pos = ftell(f);
        if (pos % 4 != 0)
            fseek(f, 4 - (pos % 4), SEEK_CUR);
    }

    fclose(f);
    return 0;
}
