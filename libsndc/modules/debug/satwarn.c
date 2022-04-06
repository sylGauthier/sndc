#include <stdio.h>

#include <sndc.h>
#include <modules/utils.h>

static int satwarn_process(struct Node* n);

/* DECLARE_MODULE(satwarn) */
const struct Module satwarn = {
    "satwarn", "debug",
    "Outputs warning in stderr if input buffer is saturating",
    {
        {"in",  DATA_BUFFER,    REQUIRED,   "input buffer to check"}
    },
    {
        {"out", DATA_BUFFER,    REQUIRED,   "passthrough of input"}
    },
    NULL,
    satwarn_process,
    NULL
};

enum SatwarnInputType {
    INP,

    NUM_INPUTS
};

static int satwarn_process(struct Node* n) {
    unsigned int i, size, numSat = 0;
    float* data;

    GENERIC_CHECK_INPUTS(n, satwarn);

    n->outputs[0] = n->inputs[0];
    size = n->inputs[0]->content.buf.size;
    data = n->inputs[0]->content.buf.data;

    for (i = 0; i < size; i++) {
        if (data[i] > 1. || data[i] < -1.) {
            numSat++;
        }
    }

    if (numSat) {
        fprintf(stderr, "%s: SATURATION WARNING: %d samples are saturated\n",
                        n->name, numSat);
    }
    return 1;
}
