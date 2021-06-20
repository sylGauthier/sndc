#include <stdlib.h>
#include <string.h>

#include "sndc.h"
#include "tokens.h"
#include "parser.h"

enum ParseError {
    ERR_NO,
    ERR_EOF,
    ERR_TOKEN,
    ERR_OTHER
};

static const char* tokenNames[] = {
    "EOF",

    "'{'",
    "'}'",
    "':'",
    "';'",
    "'.'",
    "'-'",

    "decimal literal",
    "hex literal",
    "octal literal",
    "binary literal",
    "float literal",
    "string",

    "import",
    "export",
    "as",
    "input",
    "output",

    "ident",
    "unknown"
};

static void invalid_token(int token, int expect) {
    if (token == END) {
        fprintf(stderr, "Error: unexpected end of file\n");
    } else {
        fprintf(stderr, "Error: line %d: invalid token: '%s'",
                        yylineno, tokenNames[token]);
        if (expect != UNKNOWN) {
            fprintf(stderr, " (expected %s)", tokenNames[expect]);
        }
        fprintf(stderr, "\n");
    }
}

char* str_cpy(const char* s) {
    char* res;

    if ((res = malloc(strlen(s) + 1))) {
        strcpy(res, s);
    }
    return res;
}

static int get_node(const struct SNDCFile* f, const char* name) {
    int i = 0;

    for (i = 0; i < f->numEntries; i++) {
        if (!strcmp(f->entries[i].name, name)) {
            return i;
        }
    }
    return -1;
}

#define NEW_ELEM_FUNC(n, t1, t2, a, c, m) \
static struct t2* new_##n(struct t1* container) { \
    struct t2* res = NULL; \
    if (container->c < m) { \
        res = container->a + (container->c ++); \
        memset(res, 0, sizeof(*res)); \
    } \
    return res; \
}

NEW_ELEM_FUNC(field, Entry, Field, fields, numFields, MAX_FIELDS)

NEW_ELEM_FUNC(import, SNDCFile, Import, imports, numImport, MAX_IMPORT)
NEW_ELEM_FUNC(export, SNDCFile, Export, exports, numExport, MAX_EXPORT)
NEW_ELEM_FUNC(entry, SNDCFile, Entry, entries, numEntries, MAX_ENTRIES)


static int parse_data(struct SNDCFile* f,
                      struct Entry* n,
                      struct Field* field) {
    int token, ok = 0;

    switch ((token = yylex())) {
        case DEC_LIT:
        case HEX_LIT:
        case OCT_LIT:
        case BIN_LIT:
            field->type = FIELD_FLOAT;
            field->data.f = intVal;
            ok = 1;
            break;
        case MINUS:
            if ((token = yylex()) != FLOAT_LIT) {
                invalid_token(token, FLOAT_LIT);
                fprintf(stderr, "Note: only floats can be negative\n");
                break;
            }
            dblVal *= -1;
        case FLOAT_LIT:
            field->type = FIELD_FLOAT;
            field->data.f = dblVal;
            ok = 1;
            break;
        case STRING_LIT:
            if ((field->data.str = str_cpy(yytext + 1))) {
                field->type = FIELD_STRING;
                field->data.str[strlen(field->data.str) - 1] = '\0';
                ok = 1;
            }
            break;
        case IDENT:
            field->type = FIELD_REF;
            if (!(field->data.ref.name = str_cpy(strVal))) {
                fprintf(stderr, "Error: parse_data: can't alloc string\n");
            } else if ((token = yylex()) != DOT) {
                invalid_token(token, DOT);
            } else if ((token = yylex()) != IDENT) {
                invalid_token(token, IDENT);
            } else if (!(field->data.ref.field = str_cpy(strVal))) {
                fprintf(stderr, "Error: parse_data: can't alloc string\n");
            } else {
                ok = 1;
            }
            break;
        default:
            invalid_token(token, UNKNOWN);
            return 0;
    }
    if (!ok) return 0;
    if ((token = yylex()) != SEMICOLON) {
        invalid_token(token, SEMICOLON);
        return 0;
    }
    return 1;
}

static int parse_field(struct SNDCFile* f,
                       struct Entry* n,
                       int* err) {
    int token;
    struct Field* field = NULL;

    *err = ERR_NO;
    if ((token = yylex()) != IDENT) {
        if (token == CBRACE) {
            return 0;
        }
        invalid_token(token, IDENT);
        *err = ERR_TOKEN;
    } else if (!(field = new_field(n))) {
        *err = ERR_OTHER;
    } else if (!(field->name = str_cpy(strVal))) {
        *err = ERR_OTHER;
    } else if ((token = yylex()) != COLON) {
        invalid_token(token, COLON);
        *err = ERR_TOKEN;
    } else if (!parse_data(f, n, field)) {
        *err = ERR_OTHER;
    }
    if (*err != ERR_NO && field) {
        n->numFields--;
    }
    return *err == ERR_NO;
}

static int parse_node(struct SNDCFile* f, int* err) {
    char *name = NULL, *type = NULL;
    struct Entry* node;
    int token;

    if (get_node(f, strVal) >= 0) {
        fprintf(stderr, "Error: line %d: redefinition of node %s\n",
                        yylineno, strVal);
        *err = ERR_OTHER;
    } else if (!(name = str_cpy(strVal))) {
        *err = ERR_OTHER;
    } else if ((token = yylex()) != COLON) {
        invalid_token(token, COLON);
        *err = ERR_TOKEN;
    } else if ((token = yylex()) != IDENT) {
        invalid_token(token, IDENT);
        *err = ERR_TOKEN;
    } else if (!(type = str_cpy(strVal))) {
        *err = ERR_OTHER;
    } else if ((token = yylex()) != OBRACE) {
        invalid_token(token, OBRACE);
        *err = ERR_TOKEN;
    } else if (!(node = new_entry(f))) {
        *err = ERR_OTHER;
    } else {
        node->name = name;
        node->type = type;
        while (parse_field(f, node, err));
        switch (*err) {
            case ERR_NO:
                return 1;
            default:
                f->numEntries--;
                break;
        }
    }
    free(name);
    free(type);
    return 0;
}

static int parse_import(struct SNDCFile* f, int* err) {
    int token;
    struct Import* imp = NULL;

    if (!(imp = new_import(f))) {
        fprintf(stderr, "Error: can't add import, too many imports?\n");
    } else if ((token = yylex()) != STRING_LIT) {
        invalid_token(token, STRING_LIT);
        *err = ERR_TOKEN;
    } else if (!(imp->fileName = malloc(strlen(f->path) + strlen(yytext) + 1))
            || !strcpy(imp->fileName, f->path)
            || !strcpy(imp->fileName + strlen(f->path), yytext + 1)) {
        *err = ERR_OTHER;
    } else if ((token = yylex()) != AS) {
        invalid_token(token, AS);
        *err = ERR_TOKEN;
    } else if ((token = yylex()) != IDENT) {
        invalid_token(token, IDENT);
        *err = ERR_TOKEN;
    } else if (!(imp->importName = str_cpy(strVal))) {
        *err = ERR_OTHER;
    } else if ((token = yylex()) != SEMICOLON) {
        invalid_token(token, SEMICOLON);
        *err = ERR_TOKEN;
    } else {
        imp->fileName[strlen(imp->fileName) - 1] = '\0';
        return 1;
    }
    return 0;
}

static int parse_export(struct SNDCFile* f, int* err) {
    int token, type;
    struct Export* e = NULL;

    if (!(e = new_export(f))) {
        fprintf(stderr, "Error: can't add export field, too many exports?\n");
    } else if ((token = yylex()) != INPUT && token != OUTPUT) {
        invalid_token(token, UNKNOWN);
        *err = ERR_TOKEN;
    } else if ((type = token, (token = yylex()) != IDENT)) {
        invalid_token(token, IDENT);
        *err = ERR_TOKEN;
    } else if (!(e->ref.name = str_cpy(strVal))) {
        *err = ERR_OTHER;
    } else if ((token = yylex()) != DOT) {
        invalid_token(token, DOT);
        *err = ERR_TOKEN;
    } else if ((token = yylex()) != IDENT) {
        invalid_token(token, IDENT);
        *err = ERR_TOKEN;
    } else if (!(e->ref.field = str_cpy(strVal))) {
        *err = ERR_OTHER;
    } else if ((token = yylex()) != AS) {
        invalid_token(token, AS);
        *err = ERR_TOKEN;
    } else if ((token = yylex()) != IDENT) {
        invalid_token(token, IDENT);
        *err = ERR_TOKEN;
    } else if (!(e->symbol = str_cpy(strVal))) {
        *err = ERR_OTHER;
    } else if ((token = yylex()) != SEMICOLON) {
        invalid_token(token, SEMICOLON);
        *err = ERR_TOKEN;
    } else {
        e->type = type == INPUT ? EXP_INPUT : EXP_OUTPUT;
        return 1;
    }
    if (e) {
        free(e->symbol);
        free(e->ref.name);
        free(e->ref.field);
        f->numExport--;
    }
    return 0;
}

int parse_sndc(struct SNDCFile* file, const char* name) {
    int err, token, ok = 1;
    FILE* in;

    memset(file, 0, sizeof(*file));
    if (!(in = fopen(name, "r"))) {
        fprintf(stderr, "Error: can't open file: %s\n", name);
    } else if (!(file->path = str_cpy(name))) {
        fprintf(stderr, "Error: can't dup file name: %s\n", name);
    } else {
        char* slash;

        if ((slash = strrchr(file->path, '/'))) {
            *(slash + 1) = '\0';
        } else {
            free(file->path);
            file->path = NULL;
        }

        yyin = in;
        while (ok && (token = yylex())) {
            switch (token) {
                case IDENT:
                    if (!parse_node(file, &err)) {
                        ok = 0;
                    }
                    break;
                case IMPORT:
                    if (!parse_import(file, &err)) {
                        ok = 0;
                    }
                    break;
                case EXPORT:
                    if (!parse_export(file, &err)) {
                        ok = 0;
                    }
                    break;
                default:
                    invalid_token(token, UNKNOWN);
                    break;
            }
        }
        if (token == END) err = ERR_NO;
        yylex_destroy();
        switch (err) {
            case ERR_NO:
            case ERR_EOF:
                fclose(in);
                return 1;
            default:
                fprintf(stderr,
                        "Error: file parsing failed with error %d\n",
                        err);
        }
    }
    if (in) {
        fclose(in);
    }
    free_sndc(file);
    return 0;
}

void free_sndc(struct SNDCFile* file) {
    unsigned int i;

    for (i = 0; i < file->numImport; i++) {
        free(file->imports[i].fileName);
        free(file->imports[i].importName);
    }

    for (i = 0; i < file->numExport; i++) {
        free(file->exports[i].symbol);
        free(file->exports[i].ref.name);
        free(file->exports[i].ref.field);
    }

    for (i = 0; i < file->numEntries; i++) {
        unsigned int j;

        free(file->entries[i].name);
        free(file->entries[i].type);
        for (j = 0; j < file->entries[i].numFields; j++) {
            struct Field* f = &file->entries[i].fields[j];
            free(f->name);
            switch (f->type) {
                case FIELD_STRING:
                    free(f->data.str);
                    break;
                case FIELD_REF:
                    free(f->data.ref.name);
                    free(f->data.ref.field);
                    break;
                default:
                    break;
            }
        }
    }
    free(file->path);
}
