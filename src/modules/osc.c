#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "../sndc.h"
#include "utils.h"

#define RES 2048
#define OUT 0

static int osc_process(struct Node* n);
static int osc_setup(struct Node* n);

const struct Module osc = {
    "osc",
    {
        {"function",    DATA_STRING,                REQUIRED},
        {"freq",        DATA_FLOAT | DATA_BUFFER,   REQUIRED},
        {"offset",      DATA_FLOAT | DATA_BUFFER,   OPTIONAL},
        {"amplitude",   DATA_FLOAT | DATA_BUFFER,   OPTIONAL},
        {"param",       DATA_FLOAT,                 OPTIONAL},
        {"duration",    DATA_FLOAT,                 REQUIRED},
        {"sampling",    DATA_FLOAT,                 OPTIONAL},
        {"interp",      DATA_STRING,                OPTIONAL}
    },
    {
        {"out",         DATA_BUFFER,                REQUIRED}
    },
    osc_setup,
    osc_process,
    NULL
};

const char* funcNames[] = {
    "sin",
    "square",
    "saw",
    NULL
};

const char* interpNames[] = {
    "step",
    "linear",
    "sine",
    NULL
};

enum OscInputType {
    FUN,
    FRQ,
    OFF,
    AMP,
    PRM,
    DUR,
    SPL,
    ITP,
    NUM_INPUTS
};

static int osc_setup(struct Node* n) {
    unsigned int i;
    struct Buffer* out;

    if (NUM_INPUTS > MAX_INPUTS) {
        fprintf(stderr, "Error: %s: NUM_INPUTS (%d) exceeds MAX_INPUTS (%d)\n",
                        osc.name, NUM_INPUTS, MAX_INPUTS);
        return 0;
    }
    for (i = 0; i < NUM_INPUTS; i++) {
        if (!data_valid(n->inputs[i], osc.inputs + i, n->name)) {
            return 0;
        }
    }

    if (       !data_string_valid(n->inputs[FUN],
                                  funcNames,
                                  osc.inputs[FUN].name,
                                  n->name)
            || (n->inputs[ITP] && !data_string_valid(n->inputs[ITP],
                                                     interpNames,
                                                     osc.inputs[ITP].name,
                                                     n->name))) {
        return 0;
    }

    n->outputs[OUT]->type = DATA_BUFFER;
    out = &n->outputs[OUT]->content.buf;

    if (n->inputs[SPL]) out->samplingRate = n->inputs[SPL]->content.f;
    else out->samplingRate = 44100;

    if (!n->inputs[ITP]) out->interp = INTERP_LINEAR;
    else if ((out->interp = data_parse_interp(n->inputs[ITP])) < 0) return 0;

    out->size = n->inputs[DUR]->content.f * out->samplingRate;
    return 1;
}

static void make_sin(float* buf, unsigned int size) {
    unsigned int i;
    float t;

    for (i = 0; i < size; i++) {
        t = (float) i / (float) size;
        buf[i] = sin(2 * M_PI * t);
    }
}

static void make_saw(float* buf, unsigned int size, float param) {
    unsigned int i, s1, s2;
    float t, dt1, dt2;

    s1 = 0.5 * param * (float) size;
    if (s1 == 0) s1 = 1;
    s2 = (1. - 0.5 * param) * (float) size;
    if (s2 == size) s2--;
    if (s1 == s2) s1--;
    dt1 = 1. / (float) s1;
    dt2 = -2. / ((float) s2 - (float) s1);

    t = 0;
    for (i = 0; i < size; i++) {
        buf[i] = t;
        if (i < s1 || i > s2) t += dt1;
        else t += dt2;
    }
    return;
}

static void make_square(float* buf, unsigned int size, float param) {
    unsigned int i;
    float t;

    for (i = 0; i < size; i++) {
        t = (float) i / (float) size;
        buf[i] = 2 * (t < param) - 1;
    }
}

static int osc_process(struct Node* n) {
    struct Data* out = n->outputs[OUT];
    float f, s, d, t, amp, off, param, *data, wave[RES];
    const char* fun;
    unsigned int i, size;
    struct Buffer bwave;

    bwave.data = wave;
    bwave.size = RES;
    bwave.interp = INTERP_STEP;

    d = n->inputs[DUR]->content.f;
    fun = n->inputs[FUN]->content.str;
    if (n->inputs[SPL]) {
        s = n->inputs[SPL]->content.f;
    } else {
        s = 44100;
    }
    if (n->inputs[PRM]) {
        param = n->inputs[PRM]->content.f;
        if (param < 0) param = 0;
        if (param > 1) param = 1;
    } else {
        param = 0.5;
    }
    size = d * s;
    if (!(data = malloc(size * sizeof(float)))) {
        return 0;
    }
    if (!strcmp(fun, "sin")) {
        make_sin(wave, RES);
    } else if (!strcmp(fun, "saw")) {
        make_saw(wave, RES, param);
    } else if (!strcmp(fun, "square")) {
        make_square(wave, RES, param);
    } else {
        return 0;
    }

    t = 0;
    for (i = 0; i < size; i++) {
        float x = (float) i / (float) size;

        f = data_float(n->inputs[FRQ], x, 0);
        amp = data_float(n->inputs[AMP], x, 1);
        off = data_float(n->inputs[OFF], x, 0);

        data[i] = amp * interp(&bwave, t) + off;

        t += f / s;
        if (t > 1) t -= 1;
    }

    out->content.buf.data = data;
    out->content.buf.size = d * s;
    out->ready = 1;
    return 1;
}
