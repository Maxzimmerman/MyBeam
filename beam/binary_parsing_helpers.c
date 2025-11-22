#include "binary_parsing_helpers.h"

/* Read whole file into memory */
int load_file(const char *path, byte **outbuf, usize *outsize) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    byte *buf = malloc(sz);
    if (!buf) { fclose(f); return -1; }

    fread(buf, 1, sz, f);
    fclose(f);

    *outbuf = buf;
    *outsize = sz;
    return 0;
}

/* Helper: read big-endian 32-bit */
int read_be32(const byte *p, usize rem, Uint32 *val) {
    if (rem < 4) return 0;
    // we convert the 4 1byte(8bits) numbers into one 4byte(32bits) number
    *val = ((Uint32)p[0] << 24) | ((Uint32)p[1] << 16) | ((Uint32)p[2] << 8) | ((Uint32)p[3]);
    return 1;
}

// sets the p to the start and the end to the end of the file
void reader_init(Reader *r, const byte *data, usize sz) { 
    r->p = data; r->end = data + sz; 
}

// returns the remaining bytes by substracting the current from the end
usize reader_remaining(const Reader *r) { 
    return (usize)(r->end - r->p); 
}

// checks if bytes to read
// reads 8bits the bytes at *->p into out 
// returns 1 if success and 0 if not
int reader_read_u8(Reader *r, byte *out) {
    if (reader_remaining(r) < 1) return 0;
    *out = *r->p++;
    return 1;
}

// check if bytes to read
// points out to bytes at p
// moves p by len bytes
int reader_read_bytes(Reader *r, const byte **out, usize len) {
    if (reader_remaining(r) < len) return 0;
    *out = r->p;
    r->p += len;
    return 1;
}

int reader_read_i32(Reader *r, Sint32 *out) {
    if (reader_remaining(r) < 4) return 0;
    *out = (Sint32)((r->p[0] << 24) | (r->p[1] << 16) | (r->p[2] << 8) | r->p[3]);
    r->p += 4;
    return 1;
}

/* Align size up to 4 bytes (BEAM chunk padding) */
Uint32 align4(Uint32 n) {
    return (Uint32)(4 * ((n + 3) / 4));
}

//* -- add this function near your reader helpers -- */
/* Read a BEAM tagged integer (small/medium/extended). Returns 1 on success. */
int read_tagged(Reader *r, int *tag_out, usize *val_out) {
    byte len_code;
    if (!reader_read_u8(r, &len_code)) return 0;
    int tag = len_code & 0x07;
    *tag_out = tag;

    /* small immediate (one byte total) */
    if ((len_code & 0x08) == 0) {
        *val_out = (usize)(len_code >> 4);
        return 1;
    }

    /* two-byte immediate */
    if ((len_code & 0x10) == 0) {
        byte extra;
        if (!reader_read_u8(r, &extra)) return 0;
        *val_out = (usize)(((len_code >> 5) << 8) | extra);
        return 1;
    }

    /* extended: length in multiple following bytes */
    {
        int top = (len_code >> 5);
        usize count;
        if (top < 7) {
            const int size_base = 2;
            count = (usize)(top + size_base);
        } else {
            /* read nested tagged that gives (count - 9) */
            int nested_tag;
            usize nested_val;
            if (!read_tagged(r, &nested_tag, &nested_val)) return 0;
            /* we expect an unsigned here, but continue anyway */
            const int size_base = 9;
            if (nested_val >= (usize)(INT32_MAX - size_base)) return 0;
            count = nested_val + size_base;
        }

        if (reader_remaining(r) < count) return 0;
        /* data are big-endian bytes forming the integer; for atom lengths
           we expect they fit in size_t (they do in practice), so unpack */
        if (count <= sizeof(size_t)) {
            size_t acc = 0;
            const byte *data = r->p;
            for (usize i = 0; i < count; ++i) {
                acc = (acc << 8) | data[i];
            }
            r->p += count;
            *val_out = acc;
            return 1;
        } else {
            /* too large to fit — not expected for atom lengths */
            return 0;
        }
    }
}
