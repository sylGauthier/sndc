#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <sndc.h>
#include <modules/utils.h>

#define W_SIZE 2048

static int filter_process(struct Node* n);

/* DECLARE_MODULE(gaussbp) */
const struct Module gaussbp = {
    "gaussbp", "filter", "Band pass filter using gaussian convolution",
    {
        {"in",          DATA_BUFFER,                REQUIRED,
                        "input buffer to be filtered"},

        {"lfcutoff",    DATA_FLOAT | DATA_BUFFER,   REQUIRED,
                        "low frequency cutoff"},

        {"hfcutoff",    DATA_FLOAT | DATA_BUFFER,   REQUIRED,
                        "high frequency cutoff"}
    },
    {
        {"out",         DATA_BUFFER,                REQUIRED,
                        "output, filtered buffer"},
#ifdef DEBUG
        {"masksize",    DATA_BUFFER,                REQUIRED,
                        "debug: buffer of mask size"},
#endif
    },
    NULL,
    filter_process,
    NULL
};

enum FilterInputType {
    INP = 0,
    LCO,
    HCO,
    NUM_INPUTS
};

enum FilterOutput {
    OUT = 0,
#ifdef DEBUG
    MSK,
#endif
    NUM_OUTPUTS
};

static int filter_valid(struct Node* n) {
    unsigned int i;
    struct Data *in, *out;

    for (i = 0; i < NUM_INPUTS; i++) {
        if (!data_valid(n->inputs[i], gaussbp.inputs + i, n->name)) {
            return 0;
        }
    }

    in = n->inputs[INP];
    out = n->outputs[OUT];

    memcpy(out, in, sizeof(*out));
    out->content.buf.data = NULL;
#ifdef DEBUG
    {
        struct Data* mask;

        mask = n->outputs[MSK];
        memcpy(mask, in, sizeof(*mask));
        mask->content.buf.data = NULL;
    }
#endif
    return 1;
}

static void load_gaussian(float* buf, unsigned int size) {
    unsigned int i;

    for (i = 0; i < size; i++) {
        float t = 2 * ((float) i / (float) size) - 1.;

        t *= 3.; /* this way the array includes all values >= 0.01 */
        buf[i] = exp(-1. / 2 * t * t);
    }
}

static int filter_process(struct Node* n) {
    struct Buffer *in, *out, mask;
    float win[W_SIZE], lf, hf, t;
    unsigned int i, ms;
#ifdef DEBUG
    struct Buffer* outmask = &n->outputs[MSK]->content.buf;
#endif

    if (!filter_valid(n)) {
        return 0;
    }
#ifdef DEBUG
    if (!(outmask->data = malloc(outmask->size * sizeof(float)))) {
        return 0;
    }
#endif

    in = &n->inputs[INP]->content.buf;
    out = &n->outputs[0]->content.buf;

    if (!(out->data = malloc(out->size * sizeof(float)))) {
        return 0;
    }

    load_gaussian(win, W_SIZE);
    mask.data = win;
    mask.size = W_SIZE;
    mask.interp = INTERP_LINEAR;

    for (i = 0; i < out->size; i++) {
        t = (float) i / (float) out->size;
        hf = data_float(n->inputs[HCO], t, 0);
        lf = data_float(n->inputs[LCO], t, 0);

        if (hf > 0) {
            ms = out->samplingRate / hf;
            out->data[i] = convol(in, &mask, ms, i);
        } else {
            out->data[i] = in->data[i];
        }
        if (lf > 0) {
            ms = out->samplingRate / lf;
            out->data[i] -= convol(in, &mask, ms, i);
        }
#ifdef DEBUG
        outmask->data[i] = ms;
#endif
    }

    return 1;
}
