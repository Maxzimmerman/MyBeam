#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>
#include "binary_parsing_helpers.h"

typedef struct {
    int index;
    uint8_t size;
    char *value;
} Atom;

typedef struct beam_module {
    char *module_name;
    Atom* atom_table; 
    int atom_count;
} BeamModule;

// loads the whole file in memory and calls the walk_file method on it
int load(char **argv);
/* -- replace your parse_atom_chunk() with this version -- */
int parse_atom_chunk(BeamModule *bm, const byte *chunk_data, Uint32 chunk_size); 
int parse_header(const byte *buf, usize buf_size, Uint32 *total_size); 
/* Walk chunk table and find AtU8/Atom */
int walk_file(BeamModule *bm, const byte *buf, usize buf_size);
int add_name_to_module(BeamModule *bm, const char *name, usize len);
int print_module_name(BeamModule *bm);
int add_atom_to_module(BeamModule *bm, const char *atom, usize len);
int print_atoms(BeamModule *bm);