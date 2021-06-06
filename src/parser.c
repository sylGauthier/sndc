#include <stdlib.h>
#include <string.h>

#include "sndc.h"
#include "tokens.h"

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

    "ident",
    "unknown"
};

static void invalid_token(int token, int expect) {
    if (token == END) {
        fprintf(stderr, "Error: unexpected end of file\n");
    } else {
        fprintf(stderr, "Error: line %d: invalid token: %s",
                        yylineno, tokenNames[token]);
        if (expect != UNKNOWN) {
            fprintf(stderr, " (expected %s)", tokenNames[expect]);
        }
        fprintf(stderr, "\n");
    }
}

static char* str_cpy(const char* s) {
    char* res;

    if ((res = malloc(strlen(s) + 1))) {
        strcpy(res, s);
    }
    return res;
}

static int parse_data(struct Stack* s,
                      struct Node* n,
                      struct Data** data) {
    int token, slot, ok = 0;
    struct Node* ref;
    const struct Module* m;

    switch ((token = yylex())) {
        case DEC_LIT:
        case HEX_LIT:
        case OCT_LIT:
        case BIN_LIT:
            if ((*data = stack_data_new(s))) {
                (*data)->type = DATA_FLOAT;
                (*data)->content.f = intVal;
                ok = 1;
            }
            break;
        case MINUS:
            if ((token = yylex()) != FLOAT_LIT) {
                invalid_token(token, FLOAT_LIT);
                fprintf(stderr, "Note: only floats can be negative\n");
                break;
            }
            dblVal *= -1;
        case FLOAT_LIT:
            if ((*data = stack_data_new(s))) {
                (*data)->type = DATA_FLOAT;
                (*data)->content.f = dblVal;
                ok = 1;
            }
            break;
        case STRING_LIT:
            if ((       *data = stack_data_new(s))
                    && ((*data)->content.str = str_cpy(yytext + 1))) {
                (*data)->type = DATA_STRING;
                (*data)->content.str[strlen((*data)->content.str) - 1] = '\0';
                ok = 1;
            }
            break;
        case IDENT:
            if (!(ref = stack_get_node(s, strVal))) {
                fprintf(stderr, "Error: line %d: node %s not defined\n",
                                yylineno, strVal);
            } else if (ref == n) {
                fprintf(stderr, "Error: line %d: node can't link to itself\n",
                                yylineno);
            } else if (!(m = module_find(ref->module))) {
                fprintf(stderr, "Error: line %d: "
                                "node %s is an unknown module\n",
                                yylineno, ref->name);
            } else if ((token = yylex()) != DOT) {
                invalid_token(token, DOT);
            } else if ((token = yylex()) != IDENT) {
                invalid_token(token, IDENT);
            } else if ((slot = module_get_output_slot(m, strVal)) < 0) {
                fprintf(stderr, "Error: line %d: "
                                "node %s has no output named '%s'\n",
                                yylineno, ref->name, strVal);
            } else {
                *data = ref->outputs[slot];
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

static int parse_input(struct Stack* s,
                       struct Node* n,
                       const struct Module* m,
                       int* err) {
    int token, slot;
    struct Data* data;

    *err = ERR_NO;
    if ((token = yylex()) != IDENT) {
        if (token == CBRACE) {
            return 0;
        }
        invalid_token(token, IDENT);
        *err = ERR_TOKEN;
    } else if ((slot = module_get_input_slot(m, strVal)) < 0) {
        fprintf(stderr, "Error: line %d: module %s has no input '%s'\n",
                        yylineno, m->name, strVal);
        *err = ERR_OTHER;
    } else if (n->inputs[slot]) {
        fprintf(stderr, "Error: line %d: input slot is not empty\n", yylineno);
        *err = ERR_OTHER;
    } else if ((token = yylex()) != COLON) {
        invalid_token(token, COLON);
        *err = ERR_TOKEN;
    } else if (!parse_data(s, n, &data)) {
        *err = ERR_OTHER;
    } else {
        n->inputs[slot] = data;
    }
    return *err == ERR_NO;
}

static int parse_node(struct Stack* s, int* err) {
    char* name = NULL;
    const struct Module* module;
    struct Node* node;
    int token;

    token = yylex();
    if (token == END) {
        *err = ERR_EOF;
        return 0;
    } else if (token != IDENT) {
        invalid_token(token, IDENT);
        *err = ERR_TOKEN;
    } else if (stack_get_node(s, strVal)) {
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
    } else if (!(module = module_find(strVal))) {
        fprintf(stderr, "Error: line %d: unknown module: %s\n",
                        yylineno, strVal);
        *err = ERR_OTHER;
    } else if ((token = yylex()) != OBRACE) {
        invalid_token(token, OBRACE);
        *err = ERR_TOKEN;
    } else if (!(node = stack_node_new_from_module(s, name, module))) {
        *err = ERR_OTHER;
    } else {
        unsigned int nd = s->numData;

        while (parse_input(s, node, module, err));
        switch (*err) {
            case ERR_NO:
                if (!node->setup(node)) {
                    fprintf(stderr, "Error: node setup failed for %s\n",
                                    node->name);
                    s->numNodes--;
                    *err = ERR_OTHER;
                } else {
                    return 1;
                }
                break;
            default:
                s->numNodes--;
                s->numData = nd;
                break;
        }
    }
    free(name);
    return 0;
}

int stack_load(struct Stack* stack, FILE* in) {
    int err;

    yyin = in;
    while (parse_node(stack, &err));
    yylex_destroy();
    switch (err) {
        case ERR_NO:
        case ERR_EOF:
            return 1;
        default:
            fprintf(stderr, "Error: node parsing failed with error %d\n", err);
            return 0;
    }
    return 0;
}
