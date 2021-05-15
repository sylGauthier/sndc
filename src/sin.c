#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

#include "sndc.h"

#define MODULE_N "sin"

#define FRQ 0
#define DUR 1
#define SPL 2
#define OUT 0

#define FRQ_N "freq"
#define DUR_N "duration"
#define SPL_N "sampling"
#define OUT_N "out"

#define REQUIRED 1
#define OPTIONAL 0

int data_valid(struct Data* data, int type, char required,
               const char* ctx, const char* name) {
    if (!data && !required) return 1;
    if (!data) {
        fprintf(stderr, "Error: %s: %s is required\n", ctx, name);
    }
    if (data->type != type) {
        fprintf(stderr, "Error: %s: %s expects type %d but got %d\n",
                        ctx, name, type, data->type);
    }
    return 1;
}

static int sin_valid(struct Node* n) {
    return data_valid(n->inputs[FRQ], DATA_FLOAT, REQUIRED, n->name, FRQ_N)
        && data_valid(n->inputs[DUR], DATA_FLOAT, REQUIRED, n->name, DUR_N)
        && data_valid(n->inputs[SPL], DATA_FLOAT, OPTIONAL, n->name, SPL_N)
        && data_valid(n->outputs[OUT], DATA_BUFFER, REQUIRED, n->name, OUT_N);
}

static int sin_process(struct Node* n) {
    struct Data* out = n->outputs[OUT];
    float f, s, d, *data;
    unsigned int i;

    f = n->inputs[FRQ]->content.f;
    d = n->inputs[DUR]->content.f;
    s = n->inputs[SPL]->content.f;
    if (!(data = malloc(d * s * sizeof(float)))) return 0;

    for (i = d * s; i > 0; i--) {
        data[i - 1] = sin(2 * M_PI * f * ((float)(i - 1) / s));
    }
    out->content.buf.data = data;
    out->content.buf.size = d * s;
    out->ready = 1;
    return 1;
}

int sin_load(struct Module* m) {
    m->name = MODULE_N;

    m->inputNames[FRQ] = FRQ_N;
    m->inputNames[DUR] = DUR_N;
    m->inputNames[SPL] = SPL_N;

    m->outputNames[OUT] = OUT_N;

    m->valid = sin_valid;
    m->process = sin_process;
    return 1;
}
