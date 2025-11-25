#include "load.h"
#include "binary_parsing_helpers.h"

int load(char **argv) {
    BeamModule *beam_module = calloc(1, sizeof(BeamModule));;
    byte *buf;
    usize size;

    if (load_file(argv[1], &buf, &size) != 0) {
        printf("File load error\n");
        return 1;
    }

    int ok = walk_file(beam_module, buf, size);
    free(buf);

    printf("########## Loaded Module ##########\n");
    print_module_name(beam_module);
    print_atoms(beam_module);
    print_exports(beam_module);
    print_imports(beam_module);
    printf("########## Loaded Module ##########\n");

    free(beam_module);
    return ok ? 0 : 1;
}

int parse_export_chunk(BeamModule *bm, const byte *chunk_data, Uint32 chunk_size) {
    Reader r;
    reader_init(&r, chunk_data, chunk_size);

    Sint32 count;
    if(!reader_read_i32(&r, &count)) {
        fprintf(stderr, "Failed reading export count\n");
        return 0;
    }

    for (size_t i = 1; i <= (size_t)count; ++i) {
        Uint32 name_idx;
        Uint32 arity;
        Uint32 label;
        // Read name atom index
        if (!reader_read_i32(&r, &name_idx)) {
            fprintf(stderr, "Failed reading name index for export %d\n", i);
            return 0;
        }

        if (!reader_read_i32(&r, &arity)) {
            fprintf(stderr, "Failed reading arity index for export %d\n", i);
            return 0;
        }

        if (!reader_read_i32(&r, &label)) {
            fprintf(stderr, "Failed reading label for export %d\n", i);
            return 0;
        }

        // resolve name_idx into string
        const char *name = NULL;

        if (name_idx - 1 < bm->atom_count) {
            name = bm->atom_table[name_idx - 1].value;
        } else {
            name = "(invalid atom index)";
        }

        size_t length = strlen(name);
        add_export_to_module(bm, name, length, arity, label);
        //printf("  %s |  arity%u | (label=%u)\n", name, arity, label);
    }
    return 1;
}

int parse_import_chunk(BeamModule *bm, const byte *chunk_data, Uint32 chunk_size) {
    Reader r;
    reader_init(&r, chunk_data, chunk_size);

    Uint32 count;
    reader_read_i32(&r, &count);

    for(int i = 1; i <= (size_t)count; i++) {
        Uint32 module_name_idx;
        Uint32 function_name_idx;
        Uint32 arity;
        // Read name atom index
        if (!reader_read_i32(&r, &module_name_idx)) {
            fprintf(stderr, "Failed reading name index for export %d\n", i);
            return 0;
        }

        if (!reader_read_i32(&r, &function_name_idx)) {
            fprintf(stderr, "Failed reading arity index for export %d\n", i);
            return 0;
        }

        if (!reader_read_i32(&r, &arity)) {
            fprintf(stderr, "Failed reading label for export %d\n", i);
            return 0;
        }

        const char *module_name = NULL;
        const char *function_name = NULL;

        if (module_name_idx - 1 < bm->atom_count) {
            module_name = bm->atom_table[module_name_idx - 1].value;
        } else {
            module_name = "(invalid atom index)";
        }

        if (function_name_idx - 1 < bm->atom_count) {
            function_name = bm->atom_table[function_name_idx - 1].value;
        } else {
            function_name = "(invalid atom index)";
        }

        usize module_name_len = strlen(module_name);
        usize function_name_len = strlen(function_name);

        add_import_to_module(bm, module_name, module_name_len, function_name, function_name_len, arity);
    }
    return 1;
}

/* -- replace your parse_atom_chunk() with this version -- */
int parse_atom_chunk(BeamModule *bm, const byte *chunk_data, Uint32 chunk_size) {
    Reader r;
    reader_init(&r, chunk_data, chunk_size);

    Sint32 count_signed;
    // tries to read 4 bytes as big endian and stores them as unsigned 32 bit integer into count_signed
    if (!reader_read_i32(&r, &count_signed)) {
        fprintf(stderr, "Failed reading atom count\n");
        return 0;
    }

    // check if values are stored as tagged values
    // if so we use the mark it by setting long_counts to 1 and count to 0
    // if not we just use count
    int long_counts = 0;
    Sint32 count = count_signed;
    if (count < 0) {
        long_counts = 1;
        count = -count;
    }

    /*
    What it does: Computes atoms_count = count + 1, stored as size_t.

    Why: Historically BEAM atom tables are 1-based (atom index 0 reserved). 
    This line prepares a size that some code might use if it needed the count including index 0. 
    In this function the variable isn’t further used except to reflect that convention — it’s informational/defensive.
    */

    for (size_t i = 1; i <= (size_t)count; ++i) {
        // Will hold the length in bytes of the next atom name.
        usize length = 0;

        if (long_counts) {
            // if the data is tagged
            int tag;
            usize val;
            if (!read_tagged(&r, &tag, &val)) {
                fprintf(stderr, "Failed reading tagged length for atom %zu\n", i);
                return 0;
            }
            /* tag should be TAG_u (unsigned) in practice; we don't enforce here */
            length = val;
        } else {
            // each atom is one byte long
            byte blen;
            // read atom and read into blem
            if (!reader_read_u8(&r, &blen)) {
                fprintf(stderr, "Failed reading byte length for atom %zu\n", i);
                return 0;
            }
            // cvonvert the 1-byte blen to usize (implicitly) and stores into length
            length = blen;
        }

        if (reader_remaining(&r) < length) {
            fprintf(stderr, "Atom data truncated for atom %zu\n", i);
            return 0;
        }

        // will be a pointer to the start of the atom name bytes inside the chunk buffer
        const byte *s;
        if (!reader_read_bytes(&r, &s, length)) return 0;

        /* print atom (may be UTF-8) */
        //printf("  %zu: %.*s\n", i, (int)length, (const char*)s);

        if(i == 1) {
            add_name_to_module(bm, (const char*)s, (int)length);
        }
        else {
            add_atom_to_module(bm, (const char*)s, (int)length);
        }
    }
    return 1;
}

int parse_header(const byte *buf, usize buf_size, Uint32 *total_size) {
    const byte *p = buf; 

    char header[5];
    char beam[5];

    memcpy(header, p, 4); header[4] = 0;
    read_be32(buf + 4, buf_size - 4, total_size);
    memcpy(beam, p + 8, 4); beam[4] = 0;

    printf("######BEAM HEADER#######\n");
    printf("%s\n", header);
    printf("%" PRIu32 "\n", total_size);
    printf("%s\n", beam);
    printf("########################\n");

    return 1;
}

/* Walk chunk table and find AtU8/Atom */
int walk_file(BeamModule *bm, const byte *buf, usize buf_size) {
    if (memcmp(buf, "FOR1", 4) != 0) return 0;

    // declare a 32-bit unsigned variable to store the total BEAM payload size.
    Uint32 total_size;

    // parse header reads the total_size and stores it in total_size
    parse_header(buf, buf_size, &total_size);

    /*
    Reads the 32-bit big-endian size after "FOR1"

    BEAM format:
    0–3: "FOR1"
    4–7: File sizeafter this header (big-endian)
    8–11: "BEAM"

    So this reads the 4 bytes starting at buf + 4 into total_size.

    No error check here — the parser assumes valid input.
    */
    //read_be32(buf + 4, buf_size - 4, &total_size);

    /*
    Checks bytes 8–11 for the literal "BEAM".
    If missing → Not a valid BEAM file → return failure.
    */
    if (memcmp(buf + 8, "BEAM", 4) != 0) return 0;

    /*
    Initializes the chunk table scanning pointers
    Skip the first 12 bytes:
    0–3   "FOR1"
    4–7   file size
    8–11  "BEAM"
    ^---- chunk table starts here
    p starts at the first chunk header
    end points to the end of all chunks, so we don’t read past file contents
    */
    const byte *p = buf + 12;
    const byte *end = buf + 12 + total_size;

    /*
    Each chunk header is:
    4 bytes: chunk ID
    4 bytes: chunk size
    So a minimum of 8 bytes must be available.
    */
    while (p + 8 <= end) {
        /*
        Read chunk ID (e.g., "Atom", "Code", "StrT")
        The chunk name is 4 ASCII characters.
        Copy them into a 5-byte buffer and terminate with \0 for string comparison.
        Example ID strings you may see:
        "Atom"
        "AtU8"
        "Code"
        "ExpT"
        "StrT"
        */
        char id[5];
        memcpy(id, p, 4); id[4] = 0;

        /*
        Read the chunk size (big-endian)
        p + 4 points to the 4-byte size field just after the ID.
        size = number of bytes in the chunk data.
        */
        Uint32 size;
        read_be32(p + 4, end - p - 4, &size);

        // Set pointer to the chunk data
        // This points to the first byte of the chunk contents.
        const byte *chunk = p + 8;

        /*
        Found an Atom chunk — parse and exit
        Elixir modules typically use "AtU8".
        If the current chunk is one of the atom table chunks:
        Print which one we found
        Immediately call parse_atom_chunk(...)
        Immediately return its result
        This stops scanning further chunks because we only want atoms.
        */
        if (strcmp(id, "AtU8") == 0 || strcmp(id, "Atom") == 0 || strcmp(id, "AtomUTF8") == 0) {
            parse_atom_chunk(bm, chunk, size);
        }
        else if(strcmp(id, "ExpT") == 0) {
            parse_export_chunk(bm, chunk, size);
        }
        else if(strcmp(id, "ImpT") == 0) {
            parse_import_chunk(bm, chunk, size);
        }
        else {
            printf("%s\n", id);
        }
        /*
        Move to the next chunk
        BEAM chunks are padded to 4-byte alignment.
        8 bytes = ID + size header
        align4(size) gives the padded size of the chunk data
        Add them together to jump to the next chunk header

        Example:
        If a chunk has size = 5, it will be padded to 8:
        actual data length = 5 → padded length = 8
        */
        p += 8 + align4(size);
    }
    return 0;
}

int add_name_to_module(BeamModule *bm, const char *name, usize len) {
    bm->module_name = malloc(len + 1);
    memcpy(bm->module_name, name, len);
    bm->module_name[len] = '\0';
    return 0;
}

int print_module_name(BeamModule *bm) {
    printf("MODULE NAME: %s\n", bm->module_name);
}

int add_atom_to_module(BeamModule *bm, const char *atom, usize len) {
    bm->atom_table = realloc(bm->atom_table, sizeof(Atom) * (bm->atom_count + 1));
    if(!bm->atom_table) {
        perror("realloc failed");
        exit(1); 
    }

    Atom *a = &bm->atom_table[bm->atom_count];

    a->index = bm->atom_count;
    a->size = len;
    a->value = malloc(a->size + 1);
    memcpy(a->value, atom, len);
    a->value[len] = '\0';

    bm->atom_count++;

    return 1;
}

int add_export_to_module(BeamModule *bm, const byte *name, usize len, int arity, int label) {
    bm->exports = realloc(bm->exports, sizeof(ExpT) * (bm->export_count + 1));
    if(!bm->exports) {
        perror("realloc failed");
        exit(1);
    }

    ExpT *export = &bm->exports[bm->export_count];
    export->name = malloc(len + 1);
    memcpy(export->name, name, len);
    export->name[len] = '\0';
    export->arity = arity;
    export->label = label;

    bm->export_count++;

    return 1;
}

int add_import_to_module(BeamModule *bm, const byte *module_name, usize module_name_len, const byte *function_name, usize function_name_len, int arity) {    bm->imports = realloc(bm->imports, sizeof(ImpT) * (bm->import_count + 1));
    if(!bm->imports) {
        perror("realloc faild");
        exit(1);
    }

    ImpT *import = &bm->imports[bm->import_count];
    import->module_name = malloc(module_name_len + 1);
    memcpy(import->module_name, module_name, module_name_len);
    import->module_name[module_name_len] = '\0';
    
    import->function_name = malloc(function_name_len + 1);
    memcpy(import->function_name, function_name, function_name_len);
    import->function_name[function_name_len] = '\0';

    import->arity = arity;

    bm->import_count++;

    return 1;
}

int print_atoms(BeamModule *bm) {
    for (int i = 0; i < bm->atom_count; i++) {
        printf("Atom %d: size=%d, value=%s\n",
            bm->atom_table[i].index,
            bm->atom_table[i].size,
            bm->atom_table[i].value);
    }
    return 1;
}

int print_exports(BeamModule *bm) {
    for(int i = 0; i < bm->export_count; i++) {
        printf("ExpT %d: name=%s, arity=%u, label=%u\n", 
            i,
            bm->exports[i].name,
            bm->exports[i].arity,
            bm->exports[i].label
        );
    }
    return 1;
}

int print_imports(BeamModule *bm) {
    for(int i = 0; i < bm->import_count; i++) {
        printf("ImpT %d: module_name=%s, function_name=%s, arity=%u\n",
            i,
            bm->imports[i].module_name,
            bm->imports[i].function_name,
            bm->imports[i].arity
        );
    }
    return 1;
}