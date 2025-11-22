#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>

typedef uint8_t  byte;
typedef int32_t  Sint32;
typedef uint32_t Uint32;
typedef size_t   usize;

/* Read whole file into memory */
int load_file(const char *path, byte **outbuf, usize *outsize);

/* Helper: read big-endian 32-bit */
int read_be32(const byte *p, usize rem, Uint32 *val);

/* Reader struct */
typedef struct {
    const byte *p;
    const byte *end;
} Reader;

// sets the p to the start and the end to the end of the file
void reader_init(Reader *r, const byte *data, usize sz); 

// returns the remaining bytes by substracting the current from the end
usize reader_remaining(const Reader *r);

// checks if bytes to read
// reads 8bits the bytes at *->p into out 
// returns 1 if success and 0 if not
int reader_read_u8(Reader *r, byte *out);

// check if bytes to read
// points out to bytes at p
// moves p by len bytes
int reader_read_bytes(Reader *r, const byte **out, usize len); 

int reader_read_i32(Reader *r, Sint32 *out); 

/* Align size up to 4 bytes (BEAM chunk padding) */
Uint32 align4(Uint32 n);

//* -- add this function near your reader helpers -- */
/* Read a BEAM tagged integer (small/medium/extended). Returns 1 on success. */
int read_tagged(Reader *r, int *tag_out, usize *val_out); 

/* -- replace your parse_atom_chunk() with this version -- */
int parse_atom_chunk(const byte *chunk_data, Uint32 chunk_size); 

int parse_header(const byte *buf, usize buf_size, Uint32 *total_size); 

/* Walk chunk table and find AtU8/Atom */
int walk_file(const byte *buf, usize buf_size);
