#include <stdio.h>
#include <stdlib.h>

#include <sndc.h>
#include <modules/utils.h>

#define MAX_DELAYLINE_SIZE  50000

static int echo_process(struct Node* n);

/* DECLARE_MODULE(echom) */
const struct Module echom = {
    "echo", "Produces a series of exponentially decaying echoes",
    {
        {"in",      DATA_BUFFER,    REQUIRED,
                    "input buffer to apply echo to"},
        {"wet",     DATA_FLOAT,     OPTIONAL,
                    "wetness of effect"},
        {"delay",   DATA_FLOAT,     OPTIONAL,
                    "delay between echoes"},
        {"decay",   DATA_FLOAT,     OPTIONAL,
                    "decay factor of echoes"},
        {"damp",    DATA_FLOAT,     OPTIONAL,
                    "amount of low pass filtering on echoes"},
        {"duration",DATA_FLOAT,     OPTIONAL,
                    "duration of output buffer, default to input's duration"},
    },
    {
        {"out",     DATA_BUFFER,    REQUIRED,
                    "output buffer"}
    },
    NULL,
    echo_process,
    NULL
};

enum EchoInputType {
    INP,
    WET,
    DEL,
    DEC,
    DMP,
    DUR,

    NUM_INPUTS
};

struct DelayLine {
    float cache[MAX_DELAYLINE_SIZE];   /* circular buffer */
    unsigned int head;  /* cur position in the circular buffer */
    unsigned int delay; /* in samples, equals size of buffer */
};

static float delayline_out(struct DelayLine* dl) {
    dl->head = (dl->head + 1) % dl->delay;
    return dl->cache[dl->head];
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

struct Echo {
    struct DelayLine dl;
    struct OnePole filter;
    float decay;
    float wet;
};

static int echo_setup(struct Node* n, struct Echo* echo) {
    float delay = 0.5, decay = 0.4, damp = 0.2, wet = 1.;
    float sr = n->inputs[INP]->content.buf.samplingRate;
    float duration = (float) n->inputs[INP]->content.buf.size / sr;
    unsigned int outSize;
    struct Buffer* buf = &n->outputs[0]->content.buf;

    if (n->inputs[WET]) wet      = n->inputs[WET]->content.f;
    if (n->inputs[DEL]) delay    = n->inputs[DEL]->content.f;
    if (n->inputs[DEC]) decay    = n->inputs[DEC]->content.f;
    if (n->inputs[DMP]) damp     = n->inputs[DMP]->content.f;
    if (n->inputs[DUR]) duration = n->inputs[DUR]->content.f;

    if (delay * sr > MAX_DELAYLINE_SIZE) {
        fprintf(stderr, "Error: %s: delay is too big\n", n->name);
        return 0;
    }
    echo->dl.delay = delay * sr;
    echo->filter.a = 1. - damp;
    echo->filter.b = - damp;
    echo->decay = decay;
    echo->wet = wet;

    outSize = duration * sr;
    if (!(buf->data = malloc(outSize * sizeof(float)))) {
        fprintf(stderr, "Error: %s: can't malloc output buffer\n", n->name);
        return 0;
    }
    buf->size = outSize;
    buf->samplingRate = sr;

    n->outputs[0]->type = DATA_BUFFER;
    return 1;
}

static float echo_run(struct Echo* echo, float s) {
    float out;

    out = delayline_out(&echo->dl);
    out = one_pole(&echo->filter, out);
    out = echo->decay * out + s;
    delayline_in(&echo->dl, out);
    return echo->wet * out + (1. - echo->wet) * s;
}

static int echo_process(struct Node* n) {
    unsigned int inSize, outSize, lim, i;
    float *in, *out;
    struct Echo echo = {0};

    GENERIC_CHECK_INPUTS(n, echom);

    in = n->inputs[INP]->content.buf.data;
    inSize = n->inputs[INP]->content.buf.size;

    if (!echo_setup(n, &echo)) return 0;

    out = n->outputs[0]->content.buf.data;
    outSize = n->outputs[0]->content.buf.size;

    lim = outSize > inSize ? inSize : outSize;

    for (i = 0; i < lim; i++) {
        out[i] = echo_run(&echo, in[i]);
    }
    for (; i < outSize; i++) {
        out[i] = echo_run(&echo, 0);
    }
    return 1;
}
