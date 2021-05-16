#include <stdlib.h>
#include <math.h>

#include "../sndc.h"
#include "../modules.h"
#include "utils.h"

#define MODULE_N "mix"

#define IN0 0
#define IN1 1
#define IN2 2
#define IN3 3
#define OUT 0

#define IN0_N "input0"
#define IN1_N "input1"
#define IN2_N "input2"
#define IN3_N "input3"
#define OUT_N "out"

static int mix_setup(struct Node* n) {
    if (   !data_valid(n->inputs[IN0], DATA_BUFFER, REQUIRED, n->name, IN0_N)
        || !data_valid(n->inputs[IN1], DATA_BUFFER, REQUIRED, n->name, IN1_N)
        || !data_valid(n->inputs[IN2], DATA_BUFFER, OPTIONAL, n->name, IN2_N)
        || !data_valid(n->inputs[IN3], DATA_BUFFER, OPTIONAL, n->name, IN3_N)
        || !n->outputs[OUT]) {
        return 0;
    }
    n->outputs[OUT]->type = DATA_BUFFER;
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

int mix_load(struct Module* m) {
    m->name = MODULE_N;

    m->inputNames[IN0] = IN0_N;
    m->inputNames[IN1] = IN1_N;
    m->inputNames[IN2] = IN2_N;
    m->inputNames[IN3] = IN3_N;

    m->outputNames[OUT] = OUT_N;

    m->setup = mix_setup;
    m->process = mix_process;
    return 1;
}
