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
    "string literal",
    "import literal",

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


static int parse_ref(struct Field* field, char* name) {
    int token;

    field->type = FIELD_REF;
    field->data.ref.name = name;

    if ((token = yylex()) != IDENT) {
        invalid_token(token, IDENT);
    } else if (!(field->data.ref.field = str_cpy(strVal))) {
        fprintf(stderr, "Error: parse_data: can't alloc string\n");
    } else {
        return 1;
    }
    return 0;
}

static int parse_field(struct SNDCFile*, struct Entry*, int*);
static void free_entry(struct Entry*);

static int parse_sub_entry(struct SNDCFile* f,
                           struct Field* field,
                           char* name) {
    struct Entry* new = NULL;

    if (!(new = calloc(1, sizeof(*new)))) {
        fprintf(stderr, "Error: parse_sub_entry: can't alloc entry\n");
    } else {
        int err = ERR_NO;

        field->type = FIELD_NODE;
        field->data.node = new;
        new->type = name;
        new->name = NULL;

        while (parse_field(f, new, &err));

        if (err != ERR_NO) {
            free_entry(new);
            free(new);
            field->data.node = NULL;
            return 0;
        }
        return 1;
    }
    return 0;
}

static int parse_data(struct SNDCFile* f,
                      struct Entry* n,
                      struct Field* field) {
    int token, ok = 0;
    char* sv = NULL;

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
            if ((sv = str_cpy(strVal))) {
                switch ((token = yylex())) {
                    case OBRACE:
                        ok = parse_sub_entry(f, field, sv);
                        break;
                    case DOT:
                        ok = parse_ref(field, sv);
                        break;
                    default:
                        invalid_token(token, UNKNOWN);
                        break;
                }
            }
            break;
        default:
            invalid_token(token, UNKNOWN);
            return 0;
    }
    if (ok && (token = yylex()) != SEMICOLON) {
        invalid_token(token, SEMICOLON);
        ok = 0;
    }
    if (!ok) {
        free(sv);
    }
    return ok;
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

static int file_exists(const char* name) {
    FILE* f;

    if ((f = fopen(name, "r"))) {
        fclose(f);
        return 1;
    }
    return 0;
}

static char* find_import(struct SNDCFile* f) {
    int token;
    char* ret = NULL;

    switch ((token = yylex())) {
        case STRING_LIT:
            if (!(ret = malloc(strlen(f->path) + strlen(yytext) + 1))
                    || !strcpy(ret, f->path)
                    || !strcpy(ret + strlen(f->path), yytext + 1)) {
                return NULL;
            }
            ret[strlen(ret) - 1] = '\0';
            break;
        case IMPORT_LIT:
            {
                unsigned int i = 0;

                if (!(ret = malloc(MAX_PATH_LENGTH + strlen(yytext) + 2))) {
                    break;
                }

                while (i < MAX_SNDC_PATH) {
                    strcpy(ret, sndcPath[i]);
                    strcpy(ret + strlen(ret) + 1, yytext + 1);
                    ret[strlen(sndcPath[i])] = '/';
                    ret[strlen(ret) - 1] = '\0';
                    if (file_exists(ret)) {
                        return ret;
                    }
                    i++;
                }
                free(ret);
                ret = NULL;
            }
            break;
        default:
            invalid_token(token, UNKNOWN);
    }
    if (!ret) {
        fprintf(stderr, "Error: can't find imported file: %s\n", yytext);
    }
    return ret;
}

static int parse_import(struct SNDCFile* f, int* err) {
    int token;
    struct Import* imp = NULL;

    if (!(imp = new_import(f))) {
        fprintf(stderr, "Error: can't add import, too many imports?\n");
    } else if (!(imp->fileName = find_import(f))) {
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
            file->path[0] = '\0';
        }

        yyin = in;
        yylineno = 1;
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

static void free_entry(struct Entry* entry) {
    unsigned int i;

    if (entry) {
        free(entry->name);
        free(entry->type);
        for (i = 0; i < entry->numFields; i++) {
            struct Field* f = &entry->fields[i];

            free(f->name);
            switch (f->type) {
                case FIELD_STRING:
                    free(f->data.str);
                    break;
                case FIELD_REF:
                    free(f->data.ref.name);
                    free(f->data.ref.field);
                    break;
                case FIELD_NODE:
                    free_entry(f->data.node);
                    free(f->data.node);
                default:
                    break;
            }
        }
    }
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
        free_entry(file->entries + i);
    }
    free(file->path);
}
