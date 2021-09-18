#include <stdlib.h>
#include <math.h>

#include <sndc.h>
#include <modules/utils.h>

#define OUT 0

static int mix_process(struct Node* n);

/* DECLARE_MODULE(mix) */
const struct Module mix = {
    "mix", "Mixer for up to 8 input buffers",
    {
        {"input0",  DATA_BUFFER,                REQUIRED},
        {"input1",  DATA_BUFFER,                REQUIRED},
        {"input2",  DATA_BUFFER,                OPTIONAL},
        {"input3",  DATA_BUFFER,                OPTIONAL},
        {"input4",  DATA_BUFFER,                OPTIONAL},
        {"input5",  DATA_BUFFER,                OPTIONAL},
        {"input6",  DATA_BUFFER,                OPTIONAL},
        {"input7",  DATA_BUFFER,                OPTIONAL},

        {"gain0",   DATA_FLOAT | DATA_BUFFER,   OPTIONAL},
        {"gain1",   DATA_FLOAT | DATA_BUFFER,   OPTIONAL},
        {"gain2",   DATA_FLOAT | DATA_BUFFER,   OPTIONAL},
        {"gain3",   DATA_FLOAT | DATA_BUFFER,   OPTIONAL},
        {"gain4",   DATA_FLOAT | DATA_BUFFER,   OPTIONAL},
        {"gain5",   DATA_FLOAT | DATA_BUFFER,   OPTIONAL},
        {"gain6",   DATA_FLOAT | DATA_BUFFER,   OPTIONAL},
        {"gain7",   DATA_FLOAT | DATA_BUFFER,   OPTIONAL},
    },
    {
        {"out",     DATA_BUFFER,    REQUIRED}
    },
    NULL,
    mix_process,
    NULL
};

enum MixInputType {
    IN0 = 0,
    IN1,
    IN2,
    IN3,
    IN4,
    IN5,
    IN6,
    IN7,

    GN0,
    GN1,
    GN2,
    GN3,
    GN4,
    GN5,
    GN6,
    GN7,

    NUM_INPUTS
};

static int mix_valid(struct Node* n) {
    unsigned int maxSize = 0, i;

    GENERIC_CHECK_INPUTS(n, mix);
    for (i = 0; i < 8; i++) {
        if (n->inputs[i]) {
            if (n->inputs[i]->content.buf.size > maxSize) {
                maxSize = n->inputs[i]->content.buf.size;
            }
            if (n->inputs[i]->content.buf.samplingRate !=
                    n->inputs[0]->content.buf.samplingRate) {
                fprintf(stderr, "Error: %s:"
                                "input buffers have inconsistent samplingRate",
                                n->name);
                return 0;
            }
        }
    }
    n->outputs[OUT]->type = DATA_BUFFER;
    n->outputs[OUT]->content.buf.size = maxSize;
    n->outputs[OUT]->content.buf.samplingRate =
        n->inputs[IN0]->content.buf.samplingRate;
    return 1;
}

static int mix_process(struct Node* n) {
    struct Data* out = n->outputs[OUT];
    struct Data* gains[8] = {NULL};
    float *bufs[8] = {NULL};
    float* res;
    unsigned int sizes[8] = {0}, i;
    unsigned int size = 0;

    if (!mix_valid(n)) return 0;

    for (i = 0; i < 8; i++) {
        if (n->inputs[IN0 + i]) {
            bufs[i] = n->inputs[IN0 + i]->content.buf.data;
            sizes[i] = n->inputs[IN0 + i]->content.buf.size;
        }
        if (n->inputs[GN0 + i]) {
            gains[i] = n->inputs[GN0 + i];
        }
    }
    size = n->outputs[0]->content.buf.size;
    if (!(res = calloc(size, sizeof(float)))) return 0;

    for (i = 0; i < 8; i++) {
        unsigned int j;
        float t;
        for (j = 0; j < sizes[i]; j++) {
            t = (float) j / (float) sizes[i];
            res[j] += data_float(gains[i], t, 1.) * bufs[i][j];
        }
    }
    out->type = DATA_BUFFER;
    out->content.buf.data = res;
    return 1;
}
