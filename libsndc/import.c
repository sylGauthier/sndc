#include <stdlib.h>
#include <stdio.h>

#include "sndc.h"

static int import_setup(struct Node* node) {
    struct Stack* stack = NULL;
    const struct Module* mod = node->module;
    int ok = 0;

    if (!(stack = malloc(sizeof(*stack)))) {
        fprintf(stderr, "Error: %s: can't create sub stack\n", node->name);
    } else if (stack_init(stack),
               !stack_load(stack, mod->file)) {
        fprintf(stderr, "Error: %s: loading sub stack failed\n", node->name);
    } else {
        unsigned int i, ni = 0, no = 0;

        node->data = stack;

        ok = 1;
        /* create the mapping from the node's {in,out}puts to its internal
         * stack components by searching for their name / fields
         */
        for (i = 0; i < mod->file->numExport && ok; i++) {
            struct Export* e = mod->file->exports + i;
            struct Node* ref;
            int refslot;

            switch (e->type) {
                case EXP_INPUT:
                    if (!(ref = stack_get_node(stack, e->ref.name))) {
                        fprintf(stderr, "Error: in imported module %s: "
                                "can't find exported node %s\n",
                                node->name, e->ref.name);
                        ok = 0;
                    } else if ((refslot =
                                module_get_input_slot(ref->module,
                                                      e->ref.field)) < 0) {
                        fprintf(stderr, "Error: in imported module %s: "
                                "node %s doesn't have input '%s'\n",
                                node->name, e->ref.name, e->ref.field);
                        ok = 0;
                    } else {
                        if (node->inputs[ni]) {
                            ref->inputs[refslot] = node->inputs[ni];
                        }
                        ni++;
                    }
                    break;
                case EXP_OUTPUT:
                    if (!(ref = stack_get_node(stack, e->ref.name))) {
                        fprintf(stderr, "Error: in imported module %s: "
                                "can't find exported node %s\n",
                                node->name, e->ref.name);
                        ok = 0;
                    } else if ((refslot =
                                module_get_output_slot(ref->module,
                                                       e->ref.field)) < 0) {
                        fprintf(stderr, "Error: in imported module %s: "
                                "node %s doesn't have output '%s'\n",
                                node->name, e->ref.name, e->ref.field);
                        ok = 0;
                    } else {
                        node->outputs[no++] = ref->outputs[refslot];
                    }
                    break;
            }
        }
    }
    if (!ok) {
        if (stack) {
            stack_free(stack);
        }
        free(stack);
        node->data = NULL;
    }
    node->isSetup = 1;
    return ok;
}

static int import_process(struct Node* node) {
    stack_reset(node->data);
    return stack_process(node->data);
}

static int import_teardown(struct Node* node) {
    if (node->data) {
        stack_free(node->data);
        free(node->data);
    }
    return 1;
}

int module_import(struct Module* module, const char* name, const char* file) {
    int sndcOk = 0, ok = 0;

    if (!(module->file = malloc(sizeof(struct SNDCFile)))) {
        fprintf(stderr, "Error: %s: can't allocate sndc file\n", file);
    } else if (!(sndcOk = parse_sndc(module->file, file))) {
        fprintf(stderr, "Error: %s: parsing failed\n", file);
    } else {
        struct SNDCFile* f = module->file;
        unsigned int i, ni = 0, no = 0;

        module->name = name;
        module->setup = import_setup;
        module->process = import_process;
        module->teardown = import_teardown;

        ok = 1;
        for (i = 0; i < f->numExport && ok; i++) {
            switch (f->exports[i].type) {
                case EXP_INPUT:
                    if (ni < MAX_INPUTS) {
                        module->inputs[ni].type = 0;
                        module->inputs[ni].req = OPTIONAL;
                        module->inputs[ni++].name = f->exports[i].symbol;
                    } else {
                        fprintf(stderr, "Error: %s: too many input exports\n",
                                file);
                        ok = 0;
                    }
                    break;
                case EXP_OUTPUT:
                    if (no < MAX_OUTPUTS) {
                        module->inputs[no].type = 0;
                        module->outputs[no++].name = f->exports[i].symbol;
                    } else {
                        fprintf(stderr, "Error: %s: too many input exports\n",
                                file);
                        ok = 0;
                    }
                    break;
            }
        }
    }
    if (!ok) {
        if (module->file) {
            if (sndcOk) {
                free_sndc(module->file);
            }
            free(module->file);
            module->file = NULL;
        }
    }
    return ok;
}

void module_free_import(struct Module* module) {
    if (module->file) {
        free_sndc(module->file);
        free(module->file);
    }
}
