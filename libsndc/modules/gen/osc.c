#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include <sndc.h>
#include <modules/utils.h>

#define RES 2048
#define OUT 0

#define DEF_FRQ 0
#define DEF_AMP 0.5
#define DEF_PAR 0.5
#define DEF_SPL 44100

static int osc_process(struct Node* n);

/* DECLARE_MODULE(osc) */
const struct Module osc = {
    "osc", "generator", "A generator for sine, saw and square waves",
    {
        {"function",    DATA_STRING,                REQUIRED,
                        "waveform: 'sin', 'square', 'saw' or 'input'"},

        {"waveform",    DATA_BUFFER,                OPTIONAL,
                        "buffer containing waveform, "
                        "used when 'input' is specified in 'function'"},

        {"freq",        DATA_FLOAT | DATA_BUFFER,   REQUIRED,
                        "frequency in Hz"},

        {"amplitude",   DATA_FLOAT | DATA_BUFFER,   OPTIONAL,
                        "amplitude in unit"},

        {"p_offset",    DATA_FLOAT,                 OPTIONAL,
                        "period offset, in period (1. = full period)"},

        {"a_offset",    DATA_FLOAT,                 OPTIONAL,
                        "amplitude offset, a constant that gets "
                        "added to the resulting signal"},

        {"param",       DATA_FLOAT,                 OPTIONAL,
                        "the wave parameter, relevant for 'square' and 'saw'"},

        {"duration",    DATA_FLOAT,                 REQUIRED,
                        "duration of resulting signal in seconds"},

        {"sampling",    DATA_FLOAT,                 OPTIONAL,
                        "sampling rate, def 44100Hz"},

        {"interp",      DATA_STRING,                OPTIONAL,
                        "interpolation of the resulting buffer, "
                        "'step', 'linear' or 'sine'"}
    },
    {
        {"out",         DATA_BUFFER,                REQUIRED,
                        "output signal"}
    },
    NULL,
    osc_process,
    NULL
};

enum OscInputType {
    FUN, /* wave function */
    WAV, /* wave form */
    FRQ, /* frequency */
    AMP, /* amplitude */
    POF, /* period offset */
    AOF, /* amplitude offset */
    PRM, /* wave parameter */
    DUR, /* signal duration */
    SPL, /* sampling rate */
    ITP, /* interpolation to use on output */
    NUM_INPUTS
};

static const char* funcNames[] = {
    "sin",
    "square",
    "saw",
    "input",
    NULL
};

enum FuncType {
    SIN,
    SQUARE,
    SAW,
    INPUT
};

static int osc_valid(struct Node* n) {
    struct Buffer* out;

    GENERIC_CHECK_INPUTS(n, osc);

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
    out->data = NULL;
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
    float f, s, d, t, amp, aoff, param, *data, wave[RES];
    unsigned int i, size;
    struct Buffer bwave;

    if (!osc_valid(n)) {
        fprintf(stderr, "Error: %s: invalid inputs\n", n->name);
        return 0;
    }

    bwave.data = wave;
    bwave.size = RES;
    bwave.interp = INTERP_LINEAR;

    d = n->inputs[DUR]->content.f;
    if (n->inputs[SPL]) {
        s = n->inputs[SPL]->content.f;
    } else {
        s = DEF_SPL;
    }
    if (n->inputs[PRM]) {
        param = n->inputs[PRM]->content.f;
        if (param < 0) param = 0;
        if (param > 1) param = 1;
    } else {
        param = DEF_PAR;
    }
    size = d * s;
    if (!(data = malloc(size * sizeof(float)))) {
        return 0;
    }

    switch (data_which_string(n->inputs[FUN], funcNames)) {
        case SIN:
            make_sin(wave, RES);
            break;
        case SQUARE:
            make_square(wave, RES, param);
            break;
        case SAW:
            make_saw(wave, RES, param);
            break;
        case INPUT:
            if (!n->inputs[WAV]) {
                fprintf(stderr, "Error: %s: missing 'waveform'\n", n->name);
                return 0;
            }
            bwave = n->inputs[WAV]->content.buf;
            break;
        default:
            return 0;
    }

    t = data_float(n->inputs[POF], 0, 0);
    aoff = data_float(n->inputs[AOF], 0, 0);
    for (i = 0; i < size; i++) {
        float x = (float) i / (float) size;

        f = data_float(n->inputs[FRQ], x, DEF_FRQ);
        amp = data_float(n->inputs[AMP], x, DEF_AMP);

        data[i] = amp * interp(&bwave, t) + aoff;

        t += f / s;
        if (t > 1) t -= 1;
    }

    out->content.buf.data = data;
    out->content.buf.size = d * s;
    out->ready = 1;
    return 1;
}
