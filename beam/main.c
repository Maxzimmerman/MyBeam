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
    char header[4];
    uint32_t file_size;

    fread(header, 1, 4, f);        // "FOR1"
    fread(&file_size, 1, 4, f);    // total BEAM size
    file_size = ntohl(file_size);
    fread(header, 1, 4, f);  

    char chunk_id[5] = {0};
    uint32_t raw_size, chunk_size;

    while (fread(chunk_id, 1, 4, f) == 4) {
        chunk_id[4] = '\0';

        if (fread(&raw_size, 1, 4, f) != 4) break;
        chunk_size = ntohl(raw_size);

        printf("Chunk: %s (%u bytes)\n", chunk_id, chunk_size);

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

        printf("First 4 bytes of AtU8: %02X %02X %02X %02X\n",
       data[0], data[1], data[2], data[3]);


       size_t offset = 0;

        while (offset + 2 <= chunk_size) {
            uint16_t len = (data[offset] << 8) | data[offset+1];
            offset += 2;

            if (offset + len > chunk_size) break;

            printf("Atom: %.*s\n", len, data + offset);
            offset += len;
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
