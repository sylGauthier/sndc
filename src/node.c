#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sndc.h"
#include "parser.h"

void node_init(struct Node* node) {
    unsigned int i;

    for (i = 0; i < MAX_INPUTS; i++) {
        node->inputs[i] = NULL;
    }
    for (i = 0; i < MAX_OUTPUTS; i++) {
        node->outputs[i] = NULL;
    }
    node->setup = NULL;
    node->process = NULL;
    node->teardown = NULL;
}

void node_free(struct Node* node) {
    if (node->teardown) {
        node->teardown(node);
    }
    free((char*)node->name);
}

void stack_init(struct Stack* stack) {
    stack->imports = NULL;
    stack->data = NULL;
    stack->nodes = NULL;
    stack->numNodes = 0;
    stack->numData = 0;
    stack->numImports = 0;
}

void stack_free(struct Stack* stack) {
    unsigned int i;

    for (i = 0; i < stack->numImports; i++) {
        module_free_import(stack->imports[i]);
        free(stack->imports[i]);
    }
    free(stack->imports);
    for (i = 0; i < stack->numNodes; i++) {
        node_free(stack->nodes[i]);
        free(stack->nodes[i]);
    }
    free(stack->nodes);
    for (i = 0; i < stack->numData; i++) {
        data_free(stack->data[i]);
        free(stack->data[i]);
    }
    free(stack->data);
}

struct Node* stack_node_new_from_module(struct Stack* stack,
                                        const char* name,
                                        const struct Module* module) {
    void* tmp;
    struct Node* new;
    unsigned int i;

    if (!(tmp = realloc(stack->nodes,
                        (stack->numNodes + 1) * sizeof(struct Node)))) {
        return NULL;
    }
    stack->nodes = tmp;
    if (!(new = malloc(sizeof(struct Node)))) return NULL;
    stack->nodes[stack->numNodes] = new;
    stack->numNodes++;

    node_init(new);
    if (!(new->name = str_cpy(name))) {
        free(new);
        return NULL;
    }
    new->module = module;
    new->setup = module->setup;
    new->process = module->process;
    new->teardown = module->teardown;
    for (i = 0; i < MAX_OUTPUTS; i++) {
        if (module->outputs[i].name) {
            struct Data* data;

            if (!(data = stack_data_new(stack))) {
                stack->numNodes--;
                node_free(new);
                free(new);
                return NULL;
            }
            new->outputs[i] = data;
        }
    }
    return new;
}

struct Data* stack_data_new(struct Stack* stack) {
    void* tmp;
    struct Data* new;

    if (!(tmp = realloc(stack->data, (stack->numData + 1) * sizeof(void*)))) {
        return NULL;
    }
    stack->data = tmp;
    if (!(new = malloc(sizeof(struct Data)))) return NULL;
    stack->data[stack->numData] = new;
    stack->numData++;

    data_init(new);
    return new;
}

struct Module* stack_import_new(struct Stack* stack) {
    void* tmp;
    struct Module* new;

    if (!(tmp = realloc(stack->imports,
                        (stack->numImports + 1) * sizeof(void*)))) {
        return NULL;
    }
    stack->imports = tmp;
    if (!(new = calloc(1, sizeof(struct Module)))) return NULL;
    stack->imports[stack->numImports] = new;
    stack->numImports++;

    return new;
}

struct Node* stack_get_node(struct Stack* stack, const char* name) {
    unsigned int i;

    for (i = 0; i < stack->numNodes; i++) {
        if (!strcmp(stack->nodes[i]->name, name)) {
            return stack->nodes[i];
        }
    }
    return NULL;
}

int stack_process(struct Stack* stack) {
    unsigned int i;

    for (i = 0; i < stack->numNodes; i++) {
        fprintf(stderr, "Processing %s\n", stack->nodes[i]->name);
        if (!stack->nodes[i]->process(stack->nodes[i])) {
            fprintf(stderr, "Error: processing failed\n");
            return 0;
        }
    }
    return 1;
}

static int load_input(struct Stack* stack,
                      const struct Module* mod,
                      struct Node* n,
                      struct Field* f) {
    int slot;

    if ((slot = module_get_input_slot(mod, f->name)) < 0) {
        fprintf(stderr, "Error: module %s has no input '%s'\n",
                mod->name, f->name);
    } else if (n->inputs[slot]) {
        fprintf(stderr, "Error: node %s: input %s is not empty\n",
                n->name, f->name);
    } else {
        struct Data* d = NULL;
        struct Node* r;
        const struct Module* refmod;
        int refslot;

        switch (f->type) {
            case FIELD_FLOAT:
                if ((d = stack_data_new(stack))) {
                    d->type = DATA_FLOAT;
                    d->content.f = f->data.f;
                    n->inputs[slot] = d;
                    return 1;
                }
                break;
            case FIELD_STRING:
                if ((d = stack_data_new(stack))) {
                    d->type = DATA_STRING;
                    if ((d->content.str = str_cpy(f->data.str))) {
                        n->inputs[slot] = d;
                        return 1;
                    }
                }
                break;
            case FIELD_REF:
                if (!(r = stack_get_node(stack, f->data.ref.name))) {
                    fprintf(stderr, "Error: node %s not defined\n",
                            f->data.ref.name);
                } else if (r == n) {
                    fprintf(stderr, "Error: node %s is refering to itself\n",
                            n->name);
                } else if (!(refmod = r->module)) {
                    fprintf(stderr, "Error: node %s is an unknown module\n",
                            r->name);
                } else if ((refslot =
                            module_get_output_slot(refmod,
                                                   f->data.ref.field)) < 0) {
                    fprintf(stderr,
                            "Error: node %s: node %s has no output named %s\n",
                            n->name, r->name, f->data.ref.field);
                } else {
                    n->inputs[slot] = r->outputs[refslot];
                    return 1;
                }
        }
        if (d) {
            data_free(d);
            stack->numData--;
        }
    }
    return 0;
}

static struct Module* imported_module_find(struct Stack* s, const char* name) {
    unsigned int i;

    for (i = 0; i < s->numImports; i++) {
        if (s->imports[i]->name && !strcmp(s->imports[i]->name, name)) {
            return s->imports[i];
        }
    }
    return NULL;
}

int stack_load(struct Stack* stack, struct SNDCFile* file) {
    unsigned int i;
    int ok = 1;

    for (i = 0; i < file->numImport; i++) {
        struct Import* imp = file->imports + i;
        struct Module* new;

        if (!(new = stack_import_new(stack))) {
            fprintf(stderr, "Error: can't create new module\n");
            return 0;
        } else if (!module_import(new, imp->importName, imp->fileName)) {
            fprintf(stderr, "Error: module import failed\n");
            return 0;
        }
    }

    for (i = 0; i < file->numEntries && ok; i++) {
        struct Node* new;
        const struct Module* mod;
        struct Entry* e = &file->entries[i];

        if (       !(mod = imported_module_find(stack, e->type))
                && !(mod = module_find(e->type))) {
            fprintf(stderr, "Error: %s: no such module\n", e->type);
            ok = 0;
        } else if (!(new = stack_node_new_from_module(stack, e->name, mod))) {
            fprintf(stderr, "Error: can't create new node\n");
            ok = 0;
        } else {
            unsigned int j;

            for (j = 0; j < e->numFields; j++) {
                if (!(load_input(stack, mod, new, e->fields + j))) {
                    ok = 0;
                    break;
                }
            }
            if (!(ok = new->setup(new))) {
                fprintf(stderr, "Error: stack invalid\n");
            }
        }
    }
    if (!ok) stack_free(stack);
    return ok;
}
