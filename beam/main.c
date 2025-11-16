/*
 * beam_atom_parser.c
 *
 * Simple BEAM Atom-chunk extractor (C).
 *
 * Build:
 *   gcc -o beam_atom_parser beam_atom_parser.c -lz
 *
 * Usage:
 *   ./beam_atom_parser path/to/file.beam
 *
 * Notes:
 * - Supports FOR1/BEAM container.
 * - Supports Comp chunk (full-file zlib compression).
 * - Parses Atom and AtU8/AtomUTF8 forms (negative count indicates long/tagged lengths).
 * - Minimal error handling for illustration purposes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>
#include <errno.h>

typedef uint8_t  byte;
typedef int32_t  Sint32;
typedef uint32_t Uint32;
typedef int64_t  Sint64;
typedef size_t   usize;

/* Read whole file into memory */
static int load_file(const char *path, byte **outbuf, usize *outsize) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    rewind(f);
    byte *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return -1;
    }
    fclose(f);
    *outbuf = buf;
    *outsize = (size_t)sz;
    return 0;
}

/* Helper: read big-endian 32-bit */
static int read_be32(const byte *p, usize rem, Uint32 *val) {
    if (rem < 4) return 0;
    *val = ((Uint32)p[0] << 24) | ((Uint32)p[1] << 16) | ((Uint32)p[2] << 8) | ((Uint32)p[3]);
    return 1;
}

/* zlib decompress a buffer (inflate). Returns newly allocated buffer (caller free). */
static byte *zlib_decompress(const byte *in, usize in_size, usize *out_size) {
    /* We'll guess an initial size and grow if needed */
    usize cap = in_size * 4 + 1024; /* heuristic */
    byte *out = malloc(cap);
    if (!out) return NULL;

    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    zs.next_in = (Bytef *)in;
    zs.avail_in = (uInt)in_size;

    if (inflateInit(&zs) != Z_OK) { free(out); return NULL; }

    zs.next_out = out;
    zs.avail_out = (uInt)cap;

    int r;
    while ((r = inflate(&zs, Z_NO_FLUSH)) == Z_OK) {
        if (zs.avail_out == 0) {
            /* need more space */
            usize used = cap - zs.avail_out - 0; (void)used;
            cap *= 2;
            byte *tmp = realloc(out, cap);
            if (!tmp) { inflateEnd(&zs); free(out); return NULL; }
            out = tmp;
            zs.next_out = out + zs.total_out;
            zs.avail_out = (uInt)(cap - zs.total_out);
        } else {
            /* some output produced, continue */
        }
    }

    if (r != Z_STREAM_END) {
        inflateEnd(&zs);
        free(out);
        return NULL;
    }

    *out_size = zs.total_out;
    inflateEnd(&zs);

    /* shrink to fit */
    byte *shrunk = realloc(out, *out_size);
    if (shrunk) out = shrunk;
    return out;
}

/* Small reader abstraction over a memory buffer */
typedef struct {
    const byte *p;
    const byte *end;
} Reader;

static void reader_init(Reader *r, const byte *data, usize sz) { r->p = data; r->end = data + sz; }
static usize reader_remaining(const Reader *r) { return (usize)(r->end - r->p); }

static int reader_read_u8(Reader *r, byte *out) {
    if (reader_remaining(r) < 1) return 0;
    *out = *r->p++;
    return 1;
}
static int reader_read_bytes(Reader *r, const byte **out, usize len) {
    if (reader_remaining(r) < len) return 0;
    *out = r->p;
    r->p += len;
    return 1;
}
static int reader_read_i32(Reader *r, Sint32 *out) {
    if (reader_remaining(r) < 4) return 0;
    *out = (Sint32)((r->p[0] << 24) | (r->p[1] << 16) | (r->p[2] << 8) | r->p[3]);
    r->p += 4;
    return 1;
}

/*
 * Read BEAM "tagged integer" used for long lengths.
 * Mirrors the logic in OTP's beam_file.c (the subset we need).
 *
 * We return:
 *   tag_out : low 3 bits of the len_code (caller can check TAG_u or TAG_i)
 *   val_out : if small/medium integer fits in a signed machine word -> value
 *   size_out: if non-zero, pointer-data length is stored here (caller will use it to read bytes)
 *
 * For the atom-length use-case, the tagged integer will be an unsigned (TAG_u)
 * containing the length.
 */
static int read_tagged(Reader *r, int *tag_out, usize *val_out, int *size_out) {
    byte len_code;
    if (!reader_read_u8(r, &len_code)) return 0;
    int tag = len_code & 0x07;
    *tag_out = tag;

    /* small immediate */
    if ((len_code & 0x08) == 0) {
        *val_out = (usize)(len_code >> 4);
        *size_out = 0;
        return 1;
    }

    /* two byte immediate */
    if ((len_code & 0x10) == 0) {
        byte extra;
        if (!reader_read_u8(r, &extra)) return 0;
        *val_out = (usize)(((len_code >> 5) << 8) | extra);
        *size_out = 0;
        return 1;
    }

    /* extended: read N bytes, where N depends on (len_code >> 5). If that value >= 7,
       read another tagged number which gives the size-9 base. */
    {
        int top = (len_code >> 5);
        usize count;
        if (top < 7) {
            const int size_base = 2;
            count = (usize)(top + size_base);
        } else {
            /* read size prefix (a tagged number, must be TAG_u) */
            int nested_tag;
            usize nested_val;
            int nested_size;
            if (!read_tagged(r, &nested_tag, &nested_val, &nested_size)) return 0;
            if (nested_tag != 0 /* assuming TAG_u == 0 */) {
                /* we expect unsigned here for lengths; but we'll accept anyway */
            }
            const int size_base = 9;
            if (nested_val >= (usize)(INT32_MAX - size_base)) return 0;
            count = nested_val + size_base;
        }

        /* read 'count' bytes of data that compose the large integer */
        if (reader_remaining(r) < count) return 0;
        const byte *data = r->p;
        r->p += count;

        /* try to fit into a machine word if count <= sizeof(size_t) */
        if (count <= sizeof(size_t)) {
            size_t acc = 0;
            for (usize i = 0; i < count; ++i) {
                acc = (acc << 8) | data[i];
            }
            *val_out = acc;
            *size_out = 0;
            return 1;
        } else {
            /* too big; return pointer-length style (size_out non-zero) */
            *val_out = (usize)0; /* not used */
            *size_out = (int)count;
            /* NOTE: In OTP they store a pointer to the data. We already advanced the reader,
               so to match that behaviour for our use we will not attempt to expose the pointer
               here. Instead, for atom lengths we will not hit this branch in practice. */
            return 1;
        }
    }
}

/* Align size up to 4 bytes (BEAM chunk padding) */
static Uint32 align4(Uint32 n) {
    return (Uint32)(4 * ((n + 3) / 4));
}

/* Parse an Atom/AtU8 chunk from chunk_data (size bytes). Print atoms. */
static int parse_atom_chunk(const byte *chunk_data, Uint32 chunk_size) {
    Reader r;
    reader_init(&r, chunk_data, chunk_size);

    Sint32 count_signed;
    if (!reader_read_i32(&r, &count_signed)) {
        fprintf(stderr, "Failed reading atom count\n");
        return 0;
    }

    int long_counts = 0;
    Sint32 count = count_signed;
    if (count < 0) {
        long_counts = 1;
        count = -count;
    }

    /* Reserve slot 0 per BEAM semantics */
    size_t atoms_count = (size_t)count + 1;
    char **atoms = calloc(atoms_count, sizeof(char*));
    if (!atoms) return 0;

    atoms[0] = NULL; /* index 0 reserved */

    for (size_t i = 1; i <= (size_t)count; ++i) {
        usize length = 0;
        if (long_counts) {
            int tag;
            usize val;
            int size_of_big;
            if (!read_tagged(&r, &tag, &val, &size_of_big)) {
                fprintf(stderr, "Failed reading tagged length for atom %zu\n", i);
                goto fail;
            }
            if (size_of_big != 0) {
                fprintf(stderr, "Atom length too large\n");
                goto fail;
            }
            length = val;
        } else {
            byte blen;
            if (!reader_read_u8(&r, &blen)) {
                fprintf(stderr, "Failed reading byte length for atom %zu\n", i);
                goto fail;
            }
            length = blen;
        }

        if (reader_remaining(&r) < length) {
            fprintf(stderr, "Atom data truncated for atom %zu\n", i);
            goto fail;
        }

        const byte *s;
        if (!reader_read_bytes(&r, &s, length)) goto fail;
        /* copy to null-terminated string for printing */
        char *a = malloc(length + 1);
        if (!a) goto fail;
        memcpy(a, s, length);
        a[length] = '\0';
        atoms[i] = a;
    }

    /* print atoms */
    printf("Atom table (%zu atoms):\n", atoms_count);
    for (size_t i = 1; i < atoms_count; ++i) {
        printf(" %zu: %s\n", i, atoms[i] ? atoms[i] : "<nil>");
    }

    for (size_t i = 1; i < atoms_count; ++i) free(atoms[i]);
    free(atoms);
    return 1;

fail:
    for (size_t i = 1; i < atoms_count; ++i) if (atoms[i]) free(atoms[i]);
    free(atoms);
    return 0;
}

/* Walk chunks in a BEAM buffer and call parse_atom_chunk when relevant.
 * If a Comp chunk is found, decompress and recurse into the decompressed buffer.
 */
static int walk_chunks_and_parse_atoms(const byte *buf, usize buf_size) {
    /* Expect FOR1, 32-bit size, BEAM */
    if (buf_size < 12) { fprintf(stderr, "File too small\n"); return 0; }
    if (memcmp(buf, "FOR1", 4) != 0) { fprintf(stderr, "Missing FOR1 header\n"); return 0; }

    Uint32 total_size;
    if (!read_be32(buf + 4, (usize)8, &total_size)) { fprintf(stderr, "Corrupt header\n"); return 0; }
    if (memcmp(buf + 8, "BEAM", 4) != 0) { fprintf(stderr, "Missing BEAM tag\n"); return 0; }

    const byte *p = buf + 12;
    const byte *end = buf + 12 + total_size;
    if (end > buf + buf_size) end = buf + buf_size; /* be tolerant */

    while (p + 8 <= end) {
        char id[5];
        memcpy(id, p, 4); id[4] = 0;
        Uint32 size;
        if (!read_be32(p + 4, (usize)(end - p - 4), &size)) { fprintf(stderr, "Truncated chunk header\n"); return 0; }
        const byte *chunk_data = p + 8;
        if ((usize)(chunk_data + size - buf) > buf_size) { fprintf(stderr, "Truncated chunk payload for %s\n", id); return 0; }

        /* Handle Comp chunk: decompress and recurse */
        if (memcmp(id, "Comp", 4) == 0) {
            printf("Found Comp chunk: decompressing...\n");
            usize dec_size;
            byte *dec = zlib_decompress(chunk_data, size, &dec_size);
            if (!dec) { fprintf(stderr, "Decompression failed\n"); return 0; }
            int ok = walk_chunks_and_parse_atoms(dec, dec_size);
            free(dec);
            return ok;
        }

        /* If this is an atom chunk — various chunk ids exist: AtU8 (UTF-8), Atom */
        if (memcmp(id, "AtU8", 4) == 0 || memcmp(id, "Atom", 4) == 0 || memcmp(id, "AtomUTF8", 4) == 0) {
            printf("Parsing atom chunk '%s' (size %u)\n", id, size);
            if (!parse_atom_chunk(chunk_data, size)) {
                fprintf(stderr, "Failed parsing atom chunk\n");
                return 0;
            }
            /* After parsing atoms we can return success; if you want to parse
             * more chunks, continue the loop instead */
            return 1;
        }

        /* Advance to next chunk (size is padded to 4 bytes) */
        Uint32 pad = align4(size);
        p += 8 + pad;
    }

    fprintf(stderr, "No Atom chunk found\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s file.beam\n", argv[0]);
        return 2;
    }

    byte *buf;
    usize buf_size;
    if (load_file(argv[1], &buf, &buf_size) != 0) {
        fprintf(stderr, "Failed to load file '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    int ok = walk_chunks_and_parse_atoms(buf, buf_size);
    free(buf);
    return ok ? 0 : 1;
}
