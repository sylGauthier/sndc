#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "../sndc.h"
#include "utils.h"

static int noise_process(struct Node* n);
static int noise_setup(struct Node* n);

const struct Module noise = {
    "noise",
    {
        {"duration",    DATA_FLOAT,     REQUIRED},
        {"sampling",    DATA_FLOAT,     OPTIONAL},
        {"interp",      DATA_STRING,    OPTIONAL}
    },
    {
        {"out",         DATA_BUFFER,    REQUIRED}
    },
    noise_setup,
    noise_process,
    NULL
};

static int noise_setup(struct Node* n) {
    unsigned int i;
    struct Data* out;
    struct Buffer* buf;

    for (i = 0; i < 3; i++) {
        if (!data_valid(n->inputs[i], noise.inputs + i, n->name)) {
            return 0;
        }
    }

    out = n->outputs[0];
    buf = &out->content.buf;

    out->type = DATA_BUFFER;
    buf->samplingRate = data_float(n->inputs[1], 0, 44100);
    buf->size = n->inputs[0]->content.f * buf->samplingRate;
    if ((buf->interp = data_parse_interp(n->inputs[2])) < 0) {
        buf->interp = INTERP_STEP;
    }
    buf->data = NULL;
    return 1;
}

static int noise_process(struct Node* n) {
    struct Buffer* out;
    unsigned int i, a;

    out = &n->outputs[0]->content.buf;
    if (!(out->data = malloc(out->size * sizeof(float)))) {
        return 0;
    }
    for (i = 0; i < out->size; i++) {
        a = rand();
        out->data[i] = (float) a / (float) RAND_MAX * 2.0 - 1.0;
    }
    n->outputs[0]->ready = 1;
    return 1;
}
