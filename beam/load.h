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

typedef struct {
    char *name;
    int arity;
    int label;
} ExpT;

typedef struct {
    char *module_name;
    char *function_name;
    int arity;
} ImpT;

typedef struct beam_module {
    char *module_name;

    Atom* atom_table; 
    int atom_count;

    ExpT* exports;
    int export_count;

    ImpT* imports;
    int import_count;
} BeamModule;

// loads the whole file in memory and calls the walk_file method on it
int load(char **argv);
/* Walk chunk table and find AtU8/Atom */
int walk_file(BeamModule *bm, const byte *buf, usize buf_size);
// header part
int parse_header(const byte *buf, usize buf_size, Uint32 *total_size); 
int add_name_to_module(BeamModule *bm, const char *name, usize len);
int print_module_name(BeamModule *bm);

// atom chunk
/* -- replace your parse_atom_chunk() with this version -- */
int parse_atom_chunk(BeamModule *bm, const byte *chunk_data, Uint32 chunk_size); 
int add_atom_to_module(BeamModule *bm, const char *atom, usize len);
int print_atoms(BeamModule *bm);

// export chunk
int parse_export_chunk(BeamModule *bm, const byte *chunk_data, Uint32 chunk_size);
int add_export_to_module(BeamModule *bm, const byte *name, usize len, int arity, int lable);
int print_exports(BeamModule *bm);

// import chunk
int parse_import_chunk(BeamModule *bm, const byte *chunk_data, Uint32 chunk_size);
int add_import_to_module(BeamModule *bm, const byte *module_name, usize module_name_len, const byte *function_name, usize function_name_len, int arity);
int print_imports(BeamModule *bm);

// string chunk
int parse_literal_chunk(BeamModule *bm, const byte *chunk_data, Uint32 chunk_size);