#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "../sndc.h"
#include "../modules.h"
#include "utils.h"

#define MODULE_N "osc"

#define FUN 0
#define FRQ 1
#define PRM 2
#define DUR 3
#define SPL 4
#define OUT 0

#define FUN_N "function"
#define FRQ_N "freq"
#define PRM_N "param"
#define DUR_N "duration"
#define SPL_N "sampling"
#define OUT_N "out"

#define REQUIRED 1
#define OPTIONAL 0

#define RES 2048

static int osc_setup(struct Node* n) {
    char* fun;

    if (   !data_valid(n->inputs[FRQ], DATA_FLOAT | DATA_BUFFER,
                       REQUIRED, n->name, FRQ_N)
        || !data_valid(n->inputs[PRM], DATA_FLOAT, OPTIONAL, n->name, PRM_N)
        || !data_valid(n->inputs[DUR], DATA_FLOAT, REQUIRED, n->name, DUR_N)
        || !data_valid(n->inputs[SPL], DATA_FLOAT, OPTIONAL, n->name, SPL_N)
        || !data_valid(n->inputs[FUN], DATA_STRING, REQUIRED, n->name, FUN_N)
        || !n->outputs[OUT]) {
        return 0;
    }
    fun = n->inputs[FUN]->content.str;
    if (strcmp(fun, "sin") && strcmp(fun, "saw") && strcmp(fun, "square")) {
        fprintf(stderr, "Error: %s: "
                        "%s must be one of 'sin', 'saw', 'square'\n",
                        n->name, FUN_N);
        return 0;
    }
    n->outputs[OUT]->type = DATA_BUFFER;
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
    float f, s, d, t, param, *data, wave[RES];
    const char* fun;
    unsigned int i, size;

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
    if (n->inputs[FRQ]->type == DATA_FLOAT) {
        float dt;
        f = n->inputs[FRQ]->content.f;
        dt = f / s;
        for (i = 0; i < size; i++) {
            data[i] = interp(wave, RES, t);
            t += dt;
            if (t >= 1) t -= 1;
        }
    } else if (n->inputs[FRQ]->type == DATA_BUFFER) {
        float* fbuf = n->inputs[FRQ]->content.buf.data;
        unsigned int fsize = n->inputs[FRQ]->content.buf.size;
        for (i = 0; i < size; i++) {
            f = interp(fbuf, fsize, (float) i / (float) size);
            data[i] = interp(wave, RES, t);
            t += f / s;
            if (t > 1) t -= 1;
        }
    } else {
        return 0;
    }

    out->content.buf.data = data;
    out->content.buf.size = d * s;
    out->ready = 1;
    return 1;
}

static int osc_teardown(struct Node* n) {
    return 1;
}

int osc_load(struct Module* m) {
    m->name = MODULE_N;

    m->inputNames[FUN] = FUN_N;
    m->inputNames[PRM] = PRM_N;
    m->inputNames[FRQ] = FRQ_N;
    m->inputNames[DUR] = DUR_N;
    m->inputNames[SPL] = SPL_N;

    m->outputNames[OUT] = OUT_N;

    m->setup = osc_setup;
    m->process = osc_process;
    m->teardown = osc_teardown;
    return 1;
}
