#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "../sndc.h"
#include "utils.h"

static int env_process(struct Node* n);
static int env_setup(struct Node* n);

const struct Module env = {
    "envelop",
    {
        {"input",       DATA_BUFFER,    REQUIRED},
        {"attack",      DATA_FLOAT,     REQUIRED},
        {"sustain",     DATA_FLOAT,     REQUIRED},
        {"decay",       DATA_FLOAT,     REQUIRED}
    },
    {
        {"out",         DATA_BUFFER,    REQUIRED}
    },
    env_setup,
    env_process,
    NULL
};

enum EnvInputType {
    INP,
    ATK,
    SUS,
    DEC,
    NUM_INPUTS
};

static int env_setup(struct Node* n) {
    unsigned int i;
    struct Data *in, *out;

    for (i = 0; i < NUM_INPUTS; i++) {
        if (!data_valid(n->inputs[i], env.inputs + i, n->name)) {
            return 0;
        }
    }

    in = n->inputs[INP];
    out = n->outputs[0];

    memcpy(out, in, sizeof(*out));
    out->content.buf.data = NULL;
    return 1;
}

static int env_process(struct Node* n) {
    struct Buffer *in, *out;
    unsigned int i, susi, deci, flati;
    float atkt, sust, dect;

    in = &n->inputs[INP]->content.buf;
    out = &n->outputs[0]->content.buf;

    atkt = n->inputs[ATK]->content.f;
    sust = n->inputs[SUS]->content.f;
    dect = n->inputs[DEC]->content.f;

    susi = atkt * in->samplingRate;
    deci = (atkt + sust) * in->samplingRate;
    flati = (atkt + sust + dect) * in->samplingRate;

    susi = susi > in->size ? in->size : susi;
    deci = deci > in->size ? in->size : deci;
    flati = flati > in->size ? in->size : flati;

    if (!(out->data = malloc(out->size * sizeof(float)))) {
        return 0;
    }
    for (i = 0; i < susi; i++) {
        out->data[i] = interpf(INTERP_SINE,
                               0, 1, (float) i / (float) susi) * in->data[i];
    }
    for (i = susi; i < deci; i++) {
        out->data[i] = in->data[i];
    }
    for (i = deci; i < flati; i++) {
        out->data[i] =
            interpf(INTERP_SINE, 1, 0,
                    (float) (i - deci) / (float) (flati - deci)) * in->data[i];
    }
    for (i = flati; i < out->size; i++) {
        out->data[i] = 0;
    }
    n->outputs[0]->ready = 1;
    return 1;
}
