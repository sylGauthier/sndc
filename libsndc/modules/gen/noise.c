#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include <sndc.h>
#include <modules/utils.h>
#include <modules/rand_tools.h>

static int noise_process(struct Node* n);

/* DECLARE_MODULE(noise) */
const struct Module noise = {
    "noise", "generator", "Noise generator",
    {
        {"duration",    DATA_FLOAT,     REQUIRED,
                        "duration of resulting signal"},

        {"sampling",    DATA_FLOAT,     OPTIONAL,
                        "sampling rate, def 44100Hz"},

        {"interp",      DATA_STRING,    OPTIONAL,
                        "interpolation of resulting buffer, "
                        "'step', 'linear' or 'sine'"}
    },
    {
        {"out",         DATA_BUFFER,    REQUIRED, "output signal"}
    },
    NULL,
    noise_process,
    NULL
};

static int noise_valid(struct Node* n) {
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
    unsigned int i;
    struct MTRand rng;

    if (!noise_valid(n)) return 0;
    mt_rand_init(&rng, 1);

    out = &n->outputs[0]->content.buf;
    if (!(out->data = malloc(out->size * sizeof(float)))) {
        return 0;
    }
    for (i = 0; i < out->size; i++) {
        out->data[i] = runif(&rng, -1, 1);
    }
    n->outputs[0]->ready = 1;
    return 1;
}
