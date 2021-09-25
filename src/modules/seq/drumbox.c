#include <stdlib.h>
#include <stdio.h>

#include <sndc.h>
#include <modules/utils.h>

static int drumbox_process(struct Node* n);

/* DECLARE_MODULE(drumbox) */
const struct Module drumbox = {
    "drumbox", "Simple drum machine that sequences its input samples",
    {
        {"bpm",         DATA_FLOAT,     REQUIRED,
                        "tempo in beats per minute"},
        {"divs",        DATA_FLOAT,     OPTIONAL,
                        "number of subdivision per beat, def 4"},

        {"sample0",     DATA_BUFFER,    REQUIRED, "sample #0"},
        {"sample1",     DATA_BUFFER,    OPTIONAL, "sample #1"},
        {"sample2",     DATA_BUFFER,    OPTIONAL, "sample #2"},
        {"sample3",     DATA_BUFFER,    OPTIONAL, "sample #3"},
        {"sample4",     DATA_BUFFER,    OPTIONAL, "sample #4"},
        {"sample5",     DATA_BUFFER,    OPTIONAL, "sample #5"},
        {"sample6",     DATA_BUFFER,    OPTIONAL, "sample #6"},

        {"seq0",        DATA_STRING,    REQUIRED,
                        "sequence #0, "
                        "an 'x' triggers sample0, "
                        "a '-' indicates a silent beat, "
                        "all other characters are ignored"},
        {"seq1",        DATA_STRING,    OPTIONAL, "sequence #1"},
        {"seq2",        DATA_STRING,    OPTIONAL, "sequence #2"},
        {"seq3",        DATA_STRING,    OPTIONAL, "sequence #3"},
        {"seq4",        DATA_STRING,    OPTIONAL, "sequence #4"},
        {"seq5",        DATA_STRING,    OPTIONAL, "sequence #5"},
        {"seq6",        DATA_STRING,    OPTIONAL, "sequence #6"},
    },
    {
        {"out",         DATA_BUFFER,    REQUIRED, "output full sequence"}
    },
    NULL,
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

static int drumbox_valid(struct Node* n) {
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

    if (!drumbox_valid(n)) return 0;

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
