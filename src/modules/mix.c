#include <stdlib.h>
#include <math.h>

#include "../sndc.h"
#include "utils.h"

#define IN0 0
#define IN1 1
#define IN2 2
#define IN3 3
#define OUT 0

static int mix_process(struct Node* n);
static int mix_setup(struct Node* n);

const struct Module mix = {
    "mix",
    {
        {"input0",  DATA_BUFFER,    REQUIRED},
        {"input1",  DATA_BUFFER,    REQUIRED},
        {"input2",  DATA_BUFFER,    OPTIONAL},
        {"input3",  DATA_BUFFER,    OPTIONAL}
    },
    {
        {"out",     DATA_BUFFER,    REQUIRED}
    },
    mix_setup,
    mix_process,
    NULL
};

static int mix_setup(struct Node* n) {
    unsigned int maxSize = 0, i;

    for (i = 0; i < 4; i++) {
        if (!data_valid(n->inputs[i], mix.inputs + i, n->name)) {
            return 0;
        }
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
        n->inputs[0]->content.buf.samplingRate;
    return 1;
}

static int mix_process(struct Node* n) {
    struct Data* out = n->outputs[OUT];
    float* bufs[4] = {NULL};
    float* res;
    float maxval = 0;
    unsigned int sizes[4] = {0}, sr[4] = {0}, i, numInput = 2;
    unsigned int maxSize = 0;

    bufs[0] = n->inputs[IN0]->content.buf.data;
    sizes[0] = n->inputs[IN0]->content.buf.size;
    sr[0] = n->inputs[IN0]->content.buf.samplingRate;

    bufs[1] = n->inputs[IN1]->content.buf.data;
    sizes[1] = n->inputs[IN1]->content.buf.size;
    sr[1] = n->inputs[IN1]->content.buf.samplingRate;

    if (n->inputs[IN2]) {
        bufs[2] = n->inputs[IN2]->content.buf.data;
        sizes[2] = n->inputs[IN2]->content.buf.size;
        sr[2] = n->inputs[IN2]->content.buf.samplingRate;
        numInput++;
        if (n->inputs[IN3]) {
            bufs[3] = n->inputs[IN3]->content.buf.data;
            sizes[3] = n->inputs[IN3]->content.buf.size;
            sr[3] = n->inputs[IN3]->content.buf.samplingRate;
            numInput++;
        }
    }
    for (i = 0; i < numInput; i++) {
        if (sr[i] != sr[0]) {
            fprintf(stderr, "Error: %s: inputs have different sampling rates\n",
                            n->name);
            return 0;
        }
    }
    for (i = 0; i < numInput; i++) if (sizes[i] > maxSize) maxSize = sizes[i];
    if (!(res = malloc(maxSize * sizeof(float)))) return 0;
    for (i = 0; i < maxSize; i++) {
        res[i] = 0;
        if (i < sizes[0]) res[i] += bufs[0][i];
        if (i < sizes[1]) res[i] += bufs[1][i];
        if (i < sizes[2]) res[i] += bufs[2][i];
        if (i < sizes[3]) res[i] += bufs[3][i];
        if (fabs(res[i]) > maxval) maxval = fabs(res[i]);
    }
    for (i = 0; i < maxSize; i++) {
        res[i] /= maxval;
    }
    out->type = DATA_BUFFER;
    out->content.buf.data = res;
    out->content.buf.size = maxSize;
    out->content.buf.samplingRate = sr[0];
    return 1;
}
