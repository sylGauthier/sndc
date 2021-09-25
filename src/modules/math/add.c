#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <sndc.h>
#include <modules/utils.h>

static int add_process(struct Node* n);

/* DECLARE_MODULE(add) */
const struct Module add = {
    "add", "Add 2 signals or numbers",
    {
        {"input0",  DATA_BUFFER | DATA_FLOAT,   REQUIRED, "input #0"},
        {"input1",  DATA_BUFFER | DATA_FLOAT,   REQUIRED, "input #1"},
    },
    {
        {"out",     DATA_BUFFER,                REQUIRED, "sum of inputs"}
    },
    NULL,
    add_process,
    NULL
};

static int add_valid(struct Node* n) {
    if (       !data_valid(n->inputs[0], add.inputs, n->name)
            || !data_valid(n->inputs[1], add.inputs + 1, n->name)) {
        return 0;
    }
    memcpy(n->outputs[0], n->inputs[0], sizeof(struct Data));

    if (n->outputs[0]->type == DATA_BUFFER) {
        n->outputs[0]->content.buf.data = NULL;
    } else if (n->inputs[1]->type == DATA_BUFFER) {
        fprintf(stderr, "Error: %s: can't add a buffer to a float, "
                        "consider inverting inputs\n", n->name);
        return 0;
    }
    return 1;
}

static int add_process(struct Node* n) {
    struct Data *in0, *in1, *out;
    unsigned int i;

    if (!add_valid(n)) return 0;

    in0 = n->inputs[0];
    in1 = n->inputs[1];
    out = n->outputs[0];

    if (in0->type == DATA_FLOAT) {
        out->content.f = in0->content.f + in1->content.f;
        return 1;
    }
    if (!(out->content.buf.data = malloc(out->content.buf.size
                                         * sizeof(float)))) {
        return 0;
    }
    for (i = 0; i < out->content.buf.size; i++) {
        float t = (float) i / (float) out->content.buf.size;

        out->content.buf.data[i] = in0->content.buf.data[i]
                                 + data_float(in1, t, 0);
    }
    return 1;
}
