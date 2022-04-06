#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <sndc.h>
#include <modules/utils.h>

#define MAX_DELAYLINE_SIZE  2048

static int reverb_process(struct Node* n);

/* DECLARE_MODULE(reverb) */
const struct Module reverb = {
    "reverb", "effect", "Implementation of the Freeverb algorithm",
    {
        {"in",      DATA_BUFFER,                REQUIRED,
                    "input buffer to apply reverb to, "
                    "sampling rate must be 44100 Hz."},
        {"wet",     DATA_FLOAT,                 OPTIONAL,
                    "wetness of effect"},
        {"roomsize",DATA_FLOAT,                 OPTIONAL,
                    "amount of feedback"},
        {"damp",    DATA_FLOAT,                 OPTIONAL,
                    "dampness, low pass filtering of feedback"},
        {"duration",DATA_FLOAT,                 OPTIONAL,
                    "duration of output signal, defaults to input's duration"}
    },
    {
        {"out",     DATA_BUFFER,                REQUIRED,
                    "output buffer"},
        {"mask",    DATA_BUFFER,                REQUIRED,
                    "computed auto-convolution mask, for debug purposes"}
    },
    NULL,
    reverb_process,
    NULL
};

enum ReverbInputType {
    INP,
    WET,
    RSZ,
    DMP,
    DUR,

    NUM_INPUTS
};

enum ReverbOutputType {
    OUT,

    NUM_OUTPUTS
};

struct DelayLine {
    float cache[MAX_DELAYLINE_SIZE];   /* circular buffer */
    unsigned int head;  /* cur position in the circular buffer */
    unsigned int delay; /* in samples, equals size of buffer */
};

static float delayline_out(struct DelayLine* dl) {
    float res;
    res = dl->cache[(dl->head + 1) % dl->delay];
    dl->head = (dl->head + 1) % dl->delay;
    return res;
}

static void delayline_in(struct DelayLine* dl, float s) {
    dl->cache[dl->head] = s;
}

struct OnePole {
    float last;
    float a, b;
};

static float one_pole(struct OnePole* filter, float s) {
    return (filter->last = filter->a * s - filter->b * filter->last);
}

struct Freeverb {
    /* the eight parallel LBCFs */
    struct DelayLine fbs[8];
    struct OnePole lps[8];

    /* the four in series APs */
    struct DelayLine ffs[4];
    struct DelayLine fbs2[4];

    float wet, f, d, g;
};

static void setup_freeverb(struct Freeverb* fv,
                           float wet, float roomsize, float damp, float g) {
    int i;

    memset(fv, 0, sizeof(*fv));

    for (i = 0; i < 8; i++) {
        fv->lps[i].a = 1. - damp;
        fv->lps[i].b = - damp;
    }
    fv->fbs[0].delay = 1116;
    fv->fbs[1].delay = 1188;
    fv->fbs[2].delay = 1277;
    fv->fbs[3].delay = 1356;
    fv->fbs[4].delay = 1422;
    fv->fbs[5].delay = 1491;
    fv->fbs[6].delay = 1557;
    fv->fbs[7].delay = 1617;

    fv->ffs[0].delay    = 225;
    fv->fbs2[0].delay   = 225;
    fv->ffs[1].delay    = 556;
    fv->fbs2[1].delay   = 556;
    fv->ffs[2].delay    = 441;
    fv->fbs2[2].delay   = 441;
    fv->ffs[3].delay    = 341;
    fv->fbs2[3].delay   = 341;

    fv->wet = wet;
    fv->f = roomsize;
    fv->d = damp;
    fv->g = g;
}

static float freeverb_run(struct Freeverb* fv, float s) {
    int i;
    float out = 0;

    for (i = 0; i < 8; i++) {
        float d;
        d = delayline_out(&fv->fbs[i]);
        d = one_pole(&fv->lps[i], d) * fv->f + s;
        delayline_in(&fv->fbs[i], d);
        out += d / 8;
    }
    for (i = 0; i < 4; i++) {
        float d1, d2;
        d1 = delayline_out(&fv->ffs[i]);
        d2 = delayline_out(&fv->fbs2[i]);
        delayline_in(&fv->ffs[i], out);
        out = fv->g * d2 - out + (1. + fv->g) * d1;
        delayline_in(&fv->fbs2[i], out);
    }
    return out / 8;
}

static int reverb_setup(struct Node* n, struct Freeverb* fv) {
    float wet = 1., roomsize = 0.84, damp = 0.2, g = 0.5;
    float duration = (float) n->inputs[INP]->content.buf.size / 44100.;
    unsigned int size;
    struct Buffer* out = &n->outputs[OUT]->content.buf;

    if (n->inputs[INP]->content.buf.samplingRate != 44100) {
        fprintf(stderr, "Error: %s: "
                "input buffer must have a sampling rate equal to 44100Hz\n",
                n->name);
        return 0;
    }
    if (n->inputs[WET]) wet      = n->inputs[WET]->content.f;
    if (n->inputs[RSZ]) roomsize = n->inputs[RSZ]->content.f;
    if (n->inputs[DMP]) damp     = n->inputs[DMP]->content.f;
    if (n->inputs[DUR]) duration = n->inputs[DUR]->content.f;

    size = duration * 44100;
    if (!(out->data = malloc(size * sizeof(float)))) {
        fprintf(stderr, "Error: %s: can't malloc output buffer\n", n->name);
        return 0;
    }
    n->outputs[OUT]->type = DATA_BUFFER;
    out->size = size;
    out->samplingRate = 44100;

    setup_freeverb(fv, wet, roomsize, damp, g);

    return 1;
}

static int reverb_process(struct Node* n) {
    unsigned int inSize, outSize, i, lim;
    float *in, *out;
    struct Freeverb fv;

    GENERIC_CHECK_INPUTS(n, reverb);

    in = n->inputs[INP]->content.buf.data;
    inSize = n->inputs[INP]->content.buf.size;

    if (!reverb_setup(n, &fv)) return 0;

    out = n->outputs[OUT]->content.buf.data;
    outSize = n->outputs[OUT]->content.buf.size;

    lim = inSize < outSize ? inSize : outSize;
    for (i = 0; i < lim; i++) {
        out[i] = fv.wet * freeverb_run(&fv, in[i]) + (1. - fv.wet) * in[i];
    }
    for (; i < outSize; i++) {
        out[i] = fv.wet * freeverb_run(&fv, 0);
    }

    return 1;
}
