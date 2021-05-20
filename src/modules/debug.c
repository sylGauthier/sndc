#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../sndc.h"
#include "utils.h"

static int print_process(struct Node* n);
static int print_setup(struct Node* n);

const struct Module print = {
    "print",
    {
        {"input",    DATA_BUFFER,   REQUIRED},
        {"file",     DATA_STRING,   OPTIONAL},
    },
    {{0}},
    print_setup,
    print_process,
    NULL
};

static int print_setup(struct Node* n) {
    if (!data_valid(n->inputs[0], print.inputs, n->name)) return 0;
    if (!data_valid(n->inputs[1], print.inputs + 1, n->name)) return 0;
    return 1;
}

static int print_process(struct Node* n) {
    unsigned int i;
    FILE* f;
    struct Buffer* in;

    if (n->inputs[1]) {
        if (!(f = fopen(n->inputs[1]->content.str, "w"))) {
            fprintf(stderr, "Error: %s: can't open file: %s\n",
                    n->name, n->inputs[1]->content.str);
            return 0;
        }
    } else {
        f = stdout;
    }
    in = &n->inputs[0]->content.buf;
    for (i = 0; i < in->size; i++) {
        fprintf(f, "%d %f\n", i, in->data[i]);
    }
    return 1;
}
