#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include <sndc.h>
#include <modules/utils.h>

static int env_process(struct Node* n);

/* DECLARE_MODULE(env) */
const struct Module env = {
    "envelop", "effect", "Apply envelop to input signal",
    {
        {"in",          DATA_BUFFER,    REQUIRED,
                        "input buffer upon which to apply envelop"},

        {"attack",      DATA_FLOAT,     REQUIRED,
                        "attack delay in seconds"},

        {"sustain",     DATA_FLOAT,     REQUIRED,
                        "sustain delay in seconds"},

        {"decay",       DATA_FLOAT,     REQUIRED,
                        "decay delay in seconds"},

        {"interp",      DATA_STRING,    OPTIONAL,
                        "interpolation for envelop curve, "
                        "'step', 'linear' or 'sine', def 'linear'"}
    },
    {
        {"out",         DATA_BUFFER,    REQUIRED,
                        "output buffer"}
    },
    NULL,
    env_process,
    NULL
};

enum EnvInputType {
    INP,
    ATK,
    SUS,
    DEC,
    ITP,
    NUM_INPUTS
};

static int env_valid(struct Node* n) {
    unsigned int i;
    struct Data *in, *out;

    for (i = 0; i < NUM_INPUTS; i++) {
        if (!data_valid(n->inputs[i], env.inputs + i, n->name)) {
            return 0;
        }
    }
    if (n->inputs[ITP] && data_parse_interp(n->inputs[ITP]) < 0) {
        fprintf(stderr, "Error: %s: invalid interp\n", n->name);
        return 0;
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
    int interp;

    if (!env_valid(n)) return 0;

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

    if (!n->inputs[ITP]) {
        interp = INTERP_LINEAR;
    } else if ((interp = data_parse_interp(n->inputs[ITP])) < 0) {
        return 0;
    }
    if (!(out->data = malloc(out->size * sizeof(float)))) {
        return 0;
    }
    for (i = 0; i < susi; i++) {
        out->data[i] = interpf(interp,
                               0, 1, (float) i / (float) susi) * in->data[i];
    }
    for (i = susi; i < deci; i++) {
        out->data[i] = in->data[i];
    }
    for (i = deci; i < flati; i++) {
        out->data[i] =
            interpf(interp, 1, 0,
                    (float) (i - deci) / (float) (flati - deci)) * in->data[i];
    }
    for (i = flati; i < out->size; i++) {
        out->data[i] = 0;
    }
    n->outputs[0]->ready = 1;
    return 1;
}
