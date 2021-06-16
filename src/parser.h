#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>

#define MAX_ENTRIES 128
#define MAX_FIELDS  128
#define MAX_IMPORT  128
#define MAX_EXPORT  128

struct Ref {
    char* name;
    char* field;
};

struct Field {
    char* name;
    enum FieldType {
        FIELD_FLOAT,
        FIELD_STRING,
        FIELD_REF
    } type;
    union FieldData {
        float f;
        char* str;
        struct Ref ref;
    } data;
};

struct Entry {
    char* name;
    char* type;
    struct Field fields[MAX_FIELDS];
    unsigned int numFields;
};

struct Export {
    char* symbol;
    struct Ref ref;
    enum ExportType {
        EXP_INPUT,
        EXP_OUTPUT
    } type;
};

struct Import {
    char* fileName;
    char* importName;
};

struct SNDCFile {
    struct Import imports[MAX_IMPORT];
    struct Export exports[MAX_EXPORT];
    struct Entry entries[MAX_ENTRIES];
    unsigned int numEntries, numImport, numExport;
};

char* str_cpy(const char* s);
int parse_sndc(struct SNDCFile* file, FILE* in);
void free_sndc(struct SNDCFile* file);

#endif
