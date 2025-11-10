// beam_read_atoms.c
// Safe reader that dumps the start of the AtU8/Atom chunk and
// attempts the old-style parse only if values are sane.

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>  // ntohl()
#include <stdlib.h>
#include <inttypes.h>   // PRIu32

static void hex_dump(const unsigned char *p, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        if (i % 16 == 0) printf("%08zx: ", i);
        printf("%02x ", p[i]);
        if ((i % 16) == 15) printf("\n");
    }
    if (len % 16) printf("\n");
}

int main(void) {
    const char *path = "../../output_files/Elixir.FirstModule.beam";
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return 1; }

    // skip BEAM header: "FOR1" + file size + "BEAM"
    if (fseek(f, 12L, SEEK_SET) != 0) { perror("fseek"); fclose(f); return 1; }

    char chunk_id[5] = {0};
    uint32_t raw_size, chunk_size;

    while (fread(chunk_id, 1, 4, f) == 4) {
        chunk_id[4] = '\0';
        if (fread(&raw_size, 1, 4, f) != 4) { fprintf(stderr, "Truncated chunk size\n"); break; }

        chunk_size = ntohl(raw_size);
        printf("Chunk: %.4s, size: %u\n", chunk_id, chunk_size);

        // Safety: cap the amount we will read for inspection (avoid OOM on bogus sizes)
        const uint32_t cap = 1024 * 1024; // 1 MiB cap for inspection
        if (chunk_size > cap) {
            printf("Chunk size %u is > %u; will read only first %u bytes for inspection.\n",
                   chunk_size, cap, cap);
        }

        // Read min(chunk_size, cap) bytes into buffer
        uint32_t to_read = (chunk_size > cap) ? cap : chunk_size;
        unsigned char *buf = malloc((size_t)to_read);
        if (!buf) { fprintf(stderr, "malloc failed\n"); break; }

        size_t actually_read = fread(buf, 1, to_read, f);
        if (actually_read != to_read) {
            fprintf(stderr, "Warning: read %zu bytes, expected %u\n", actually_read, to_read);
        }

        // Print a small hex dump to inspect format
        printf("First %zu bytes (hex):\n", actually_read);
        hex_dump(buf, actually_read > 256 ? 256 : actually_read);

        // === Attempt the classic parse only if it looks sane ===
        // Classic Atom chunk (older tool assumptions):
        //  0..3   uint32 BE: atom_count
        //  then for each atom:
        //     4 bytes uint32 BE length
        //     length bytes name (not NUL terminated)
        //
        // We'll only attempt this if:
        //  - we read at least 4 bytes
        //  - atom_count > 0
        //  - atom_count is small enough (not larger than chunk_size / 1)
        if (actually_read >= 4) {
            uint32_t atom_count = ntohl(*(uint32_t *)buf);
            printf("Interpreting first 4 bytes as atom_count = %" PRIu32 "\n", atom_count);

            // sanity checks:
            // - atom_count should not be zero (some files may have 0)
            // - atom_count should not be huge relative to chunk_size
            //   (e.g., atom_count * 4 should be <= chunk_size)
            if (atom_count > 0 && (uint64_t)atom_count * 4 <= chunk_size && atom_count < 1000000U) {
                printf("Looks plausible to try classic parse (atom_count=%" PRIu32 ")\n", atom_count);
                size_t offset = 4;
                uint32_t i;
                for (i = 0; i < atom_count; ++i) {
                    // Need at least 4 bytes for length
                    if (offset + 4 > actually_read) {
                        printf("Need more data (length field for atom %u not in buffer), stopping\n", i+1);
                        break;
                    }
                    uint32_t len = ntohl(*(uint32_t *)(buf + offset));
                    offset += 4;
                    // Check lengths against limits
                    if ((uint64_t)offset + len > chunk_size || (uint64_t)offset + len > actually_read) {
                        printf("Atom %u length %u would run past chunk boundaries - abort parse\n", i+1, len);
                        break;
                    }
                    printf("Atom %u: %.*s\n", i+1, (int)len, buf + offset);
                    offset += len;
                }
            } else {
                printf("Not attempting classic parse: atom_count looks suspicious or chunk not matching classic layout.\n");
                printf("This likely means your BEAM uses a newer AtU8/atom table layout (OTP 28+ changes) or an alternate layout.\n");
            }
        } else {
            printf("Chunk too small to parse atom count.\n");
        }

        free(buf);

        // If we read only a capped part, we must seek the remainder of the chunk forward
        if (to_read < chunk_size) {
            long remain = (long)chunk_size - (long)to_read;
            if (fseek(f, remain, SEEK_CUR) != 0) { perror("fseek remainder"); break; }
        }

        // Align to 4-byte boundary after chunk
        long pos = ftell(f);
        if (pos == -1L) { perror("ftell"); break; }
        long pad = (4 - (pos % 4)) % 4;
        if (pad) {
            if (fseek(f, pad, SEEK_CUR) != 0) { perror("fseek pad"); break; }
        }
    }

    fclose(f);
    return 0;
}
