#include <stdlib.h>
#include <stdio.h>

#include <sndc.h>
#include <modules/utils.h>

static int drumbox_process(struct Node* n);
static int drumbox_setup(struct Node* n);

const struct Module drumbox = {
    "drumbox", "Simple drum machine that sequences its input samples",
    {
        {"bpm",         DATA_FLOAT,     REQUIRED},
        {"divs",        DATA_FLOAT,     OPTIONAL},

        {"sample0",     DATA_BUFFER,    REQUIRED},
        {"sample1",     DATA_BUFFER,    OPTIONAL},
        {"sample2",     DATA_BUFFER,    OPTIONAL},
        {"sample3",     DATA_BUFFER,    OPTIONAL},
        {"sample4",     DATA_BUFFER,    OPTIONAL},
        {"sample5",     DATA_BUFFER,    OPTIONAL},
        {"sample6",     DATA_BUFFER,    OPTIONAL},

        {"seq0",        DATA_STRING,    REQUIRED},
        {"seq1",        DATA_STRING,    OPTIONAL},
        {"seq2",        DATA_STRING,    OPTIONAL},
        {"seq3",        DATA_STRING,    OPTIONAL},
        {"seq4",        DATA_STRING,    OPTIONAL},
        {"seq5",        DATA_STRING,    OPTIONAL},
        {"seq6",        DATA_STRING,    OPTIONAL},
    },
    {
        {"out",         DATA_BUFFER,    REQUIRED}
    },
    drumbox_setup,
    drumbox_process,
    NULL
};

enum DrumboxInputType {
    BPM,
    DIVS,

    SPL0,
    SPL1,
    SPL2,
    SPL3,
    SPL4,
    SPL5,
    SPL6,

    SEQ0,
    SEQ1,
    SEQ2,
    SEQ3,
    SEQ4,
    SEQ5,
    SEQ6,

    NUM_INPUTS
};

static unsigned int seqlen(const char* seq) {
    const char* cur = seq;
    unsigned int len = 0;

    if (!seq) return 0;

    while (*cur) {
        switch (*cur) {
            case 'x':
            case '-':
                len++;
                break;
            default:
                break;
        }
        cur++;
    }
    return len;
}

static int drumbox_setup(struct Node* n) {
    struct Buffer* out;
    unsigned int i, sl, maxl = 0, rate, divsize;
    float divs, bpm;

    GENERIC_CHECK_INPUTS(n, drumbox);

    sl = seqlen(n->inputs[SEQ0]->content.str);
    rate = n->inputs[SPL0]->content.buf.samplingRate;
    maxl = n->inputs[SPL0]->content.buf.size;
    if (!sl) {
        fprintf(stderr, "Error: %s: seq0 can't be of length 0\n", n->name);
        return 0;
    }
    for (i = 1; i <= 6; i++) {
        if (       n->inputs[SEQ0 + i]
                && seqlen(n->inputs[SEQ0 + i]->content.str) != sl) {
            fprintf(stderr, "Error: %s: seq%d has inconsistent length\n",
                    n->name, i);
            return 0;
        }
        if (n->inputs[SPL0 + i]) {
            if (n->inputs[SPL0 + i]->content.buf.samplingRate != rate) {
                fprintf(stderr, "Error: %s:"
                        "sample%d has inconsistent sampling rate\n",
                        n->name, i);
                return 0;
            }
            if (n->inputs[SPL0 + i]->content.buf.size > maxl) {
                maxl = n->inputs[SPL0 + i]->content.buf.size;
            }
        }
    }

    divs = data_float(n->inputs[DIVS], 0, 4);
    bpm = data_float(n->inputs[BPM], 0, 120);

    n->outputs[0]->type = DATA_BUFFER;
    out = &n->outputs[0]->content.buf;
    out->samplingRate = rate;
    divsize = ((float) rate * 60. / (bpm * divs));
    out->size = sl * divsize + maxl;
    return 1;
}

static int drumbox_process(struct Node* n) {
    unsigned int i, dx;
    struct Buffer* out;
    float bpm, divs;

    out = &n->outputs[0]->content.buf;

    if (!(out->data = calloc(out->size, sizeof(float)))) {
        fprintf(stderr, "Error: node %s: can't allocate output buffer\n",
                n->name);
        return 0;
    }

    divs = data_float(n->inputs[DIVS], 0, 4);
    bpm = data_float(n->inputs[BPM], 0, 120);
    dx = ((float) out->samplingRate * 60. / (bpm * divs));

    for (i = 0; i <= 6; i++) {
        if (n->inputs[SEQ0 + i]) {
            char* cur = n->inputs[SEQ0 + i]->content.str;
            unsigned int x = 0;

            while (*cur) {
                switch (*cur) {
                    case '-':
                        x += dx;
                        break;
                    case 'x':
                        addbuf(out->data + x,
                               n->inputs[SPL0 + i]->content.buf.data,
                               n->inputs[SPL0 + i]->content.buf.size);
                        x += dx;
                        break;
                    default:
                        break;
                }
                cur++;
            }
        }
    }
    return 1;
}
