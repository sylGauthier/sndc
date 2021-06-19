#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <sndc.h>
#include <modules/utils.h>

static int func_setup(struct Node* n);
static int func_process(struct Node* n);

const struct Module func = {
    "func", "A generator for simple mathematical functions",
    {
        {"function",    DATA_STRING,    REQUIRED},
        {"duration",    DATA_FLOAT,     REQUIRED},
        {"sampling",    DATA_FLOAT,     OPTIONAL},
        {"interp",      DATA_STRING,    OPTIONAL},
        {"param0",      DATA_FLOAT,     OPTIONAL},
        {"param1",      DATA_FLOAT,     OPTIONAL},
        {"param2",      DATA_FLOAT,     OPTIONAL},
    },
    {
        {"out",         DATA_BUFFER,    REQUIRED}
    },
    func_setup,
    func_process,
    NULL
};

enum InputType {
    FUN = 0,
    DUR,
    SPL,
    ITP,
    PM0,
    PM1,
    PM2,
    NUM_INPUTS
};

static const char* funcNames[] = {
    "exp",
    "lin",
    "inv",
    NULL
};

enum FuncType {
    EXP = 0,
    LIN,
    INV
};

static int func_setup(struct Node* n) {
    struct Buffer* out;

    GENERIC_CHECK_INPUTS(n, func);

    if (!data_string_valid(n->inputs[FUN],
                           funcNames,
                           func.inputs[FUN].name,
                           n->name)) {
        return 0;
    }
    if (n->inputs[ITP] && !data_string_valid(n->inputs[ITP],
                                             interpNames,
                                             func.inputs[ITP].name,
                                             n->name)) {
        return 0;
    }

    n->outputs[0]->type = DATA_BUFFER;
    out = &n->outputs[0]->content.buf;

    if (n->inputs[SPL]) out->samplingRate = n->inputs[SPL]->content.f;
    else out->samplingRate = 44100;

    if (!n->inputs[ITP]) out->interp = INTERP_LINEAR;
    else if ((out->interp = data_parse_interp(n->inputs[ITP])) < 0) return 0;

    out->size = n->inputs[DUR]->content.f * out->samplingRate;
    out->data = NULL;
    return 1;
}

static void make_exp(struct Buffer* buf, float params[]) {
    unsigned int i;
    float dt = 1. / (float) buf->samplingRate;
    float t = 0;

    for (i = 0; i < buf->size; i++) {
        buf->data[i] = params[0] * expf((t + params[1]) * params[2]);
        t += dt;
    }
}

static void make_lin(struct Buffer* buf, float params[]) {
    unsigned int i;
    float dt = 1. / (float) buf->samplingRate;
    float t = 0, duration = (float) buf->size / (float) buf->samplingRate;
    float a = (params[1] - params[0]) / duration;
    float b = params[0];

    for (i = 0; i < buf->size; i++) {
        buf->data[i] = a * t + b;
        t += dt;
    }
}

static void make_inv(struct Buffer* buf, float params[]) {
    unsigned int i;
    float dt = 1. / (float) buf->samplingRate;
    float t = 0;

    for (i = 0; i < buf->size; i++) {
        buf->data[i] = params[0] / (t + params[1]);
        t += dt;
    }
}

static int func_process(struct Node* n) {
    struct Buffer* out = &n->outputs[0]->content.buf;
    float params[3];

    if (!(out->data = malloc(out->size * sizeof(float)))) {
        return 0;
    }
    switch (data_which_string(n->inputs[FUN], funcNames)) {
        case EXP:
            params[0] = data_float(n->inputs[PM0], 0, 1);
            params[1] = data_float(n->inputs[PM1], 0, 0);
            params[2] = data_float(n->inputs[PM2], 0, -1);
            make_exp(out, params);
            return 1;
        case LIN:
            params[0] = data_float(n->inputs[PM0], 0, 0);
            params[1] = data_float(n->inputs[PM1], 0, 1);
            make_lin(out, params);
            return 1;
        case INV:
            params[0] = data_float(n->inputs[PM0], 0, 1);
            params[1] = data_float(n->inputs[PM1], 0, 1);
            make_inv(out, params);
            return 1;
        default:
            return 0;
    }
    return 0;
}
