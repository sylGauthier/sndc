#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sndc.h>
#include <modules/utils.h>

static int print_process(struct Node* n);

/* DECLARE_MODULE(print) */
const struct Module print = {
    "print", "debug",
    "Prints input buffer to specified file for plotting/analysis",
    {
        {"in",       DATA_BUFFER,   REQUIRED, "input buffer"},
        {"file",     DATA_STRING,   OPTIONAL, "output file name"},
    },
    {{0}},
    NULL,
    print_process,
    NULL
};

static int print_valid(struct Node* n) {
    if (!data_valid(n->inputs[0], print.inputs, n->name)) return 0;
    if (!data_valid(n->inputs[1], print.inputs + 1, n->name)) return 0;
    return 1;
}

static int print_process(struct Node* n) {
    unsigned int i;
    FILE* f;
    struct Buffer* in;

    if (!print_valid(n)) return 0;

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
