#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <sndc.h>
#include <modules/utils.h>

static int filter_process(struct Node* n);

/* DECLARE_MODULE(simplelp) */
const struct Module simplelp = {
    "simplelp", "Apply filter to input signal",
    {
        {"in",          DATA_BUFFER,                REQUIRED},
        {"cutoff",      DATA_FLOAT | DATA_BUFFER,   REQUIRED}
    },
    {
        {"out",         DATA_BUFFER,                REQUIRED},
    },
    NULL,
    filter_process,
    NULL
};

enum FilterInputType {
    INP = 0,
    CUT,
    NUM_INPUTS
};

enum FilterOutput {
    OUT = 0,
    NUM_OUTPUTS
};

static int filter_valid(struct Node* n) {
    struct Data *in, *out;

    GENERIC_CHECK_INPUTS(n, simplelp);

    in = n->inputs[INP];
    out = n->outputs[OUT];

    memcpy(out, in, sizeof(*out));
    out->content.buf.data = NULL;
    return 1;
}

static int filter_process(struct Node* n) {
    struct Buffer *in, *out;
    struct Data* cutoff;
    float t = 0, dt, sr;
    unsigned int i;

    if (!filter_valid(n)) return 0;

    in = &n->inputs[INP]->content.buf;
    out = &n->outputs[0]->content.buf;
    cutoff = n->inputs[CUT];
    sr = in->samplingRate;

    if (!(out->data = malloc(out->size * sizeof(float)))) {
        return 0;
    }

    out->data[0] = data_float(cutoff, 0, 0) / sr * in->data[0];

    dt = 1. / (float) out->size;
    for (i = 1; i < out->size; i++) {
        float u;
        t += dt;
        u = data_float(cutoff, t, 0) / sr;
        out->data[i] = (1. - u) * out->data[i - 1] + u * in->data[i];
    }

    return 1;
}
