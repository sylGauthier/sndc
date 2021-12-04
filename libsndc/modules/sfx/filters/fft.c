#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <fftw3.h>

#include <sndc.h>
#include <modules/utils.h>

#define DEFAULT_ORDER       4
#define DEFAULT_WIN_SIZE    2048
#define GAIN_RESOL          1024

static int filter_process(struct Node* n);

/* DECLARE_MODULE(fftbp) */
const struct Module fftbp = {
    "filter", "Generic lowpass / highpass filter",
    {
        {"in",          DATA_BUFFER,                REQUIRED,
                        "input buffer to be filtered"},

        {"cutoff",      DATA_FLOAT | DATA_BUFFER,   REQUIRED,
                        "frequency cutoff"},

        {"mode",        DATA_STRING,                REQUIRED,
                        "filter mode, 'lowpass' or 'highpass'"},

        {"order",       DATA_FLOAT,                 OPTIONAL,
                        "Butterworth order, preferably an even, "
                        "positive integer, def '4'"},

        {"fftwinsize",  DATA_FLOAT,                 OPTIONAL,
                        "FFT window size, def '2048'"}
    },
    {
        {"out",         DATA_BUFFER,                REQUIRED,
                        "output, filtered buffer, same size and "
                        "sampling rate as input"},
    },
    NULL,
    filter_process,
    NULL
};

enum FilterInput {
    INP = 0,
    COF,
    MOD,
    ORD,
    FTW,
    NUM_INPUTS
};

static void setup(struct Node* n) {
    struct Data *in, *out;

    in = n->inputs[INP];
    out = n->outputs[0];
    memcpy(out, in, sizeof(*out));
    out->content.buf.data = NULL;
}

/* TODO: proper window */
static void make_window(float* win, unsigned int winSize) {
    unsigned int i;

    for (i = 0; i < winSize; i++) {
        float t = M_PI * (2. * (float) i / (float) winSize - 1.);
        win[i] = sqrt((cos(t) + 1.) / 2.);
    }
}

static void load_fftin(float* fftin,
                       float* buf, unsigned int bufSize,
                       float* win, unsigned int winSize,
                       int pos) {
    int i, max;

    max = winSize < bufSize - pos ? winSize : bufSize - pos;

    for (i = 0; i + pos < 0; i++) {
        fftin[i] = 0;
    }
    for (; i < max; i++) {
        fftin[i] = win[i] * buf[i + pos];
    }
    for (; i < winSize; i++) {
        fftin[i] = 0;
    }
}

static void export_fftin(float* fftout,
                         float* buf, unsigned int bufSize,
                         float* win, unsigned int winSize,
                         int pos) {
    int i, max;

    max = winSize < bufSize - pos ? winSize : bufSize - pos;
    i = pos < 0 ? -pos : 0;
    for (; i < max; i++) {
        buf[i + pos] += win[i] * fftout[i] / (float) winSize;
    }
}

static void apply_filter(fftwf_complex* spectre,
                         unsigned int winSize,
                         unsigned int samplingRate,
                         float f0,
                         struct Buffer* gain) {
    unsigned int i;

    if (f0 < 0) f0 = 0;
    if (f0 > samplingRate / 2) f0 = samplingRate / 2;

    for (i = 0; i < winSize / 2 + 1; i++) {
        float curfreq = (float) i / (float) winSize * (float) samplingRate;
        float t1 = curfreq / (10. * f0);
        float c;

        c = interp(gain, t1);
        spectre[i][0] *= c;
        spectre[i][1] *= c;
    }
}

enum FilterMode {
    LOW_PASS,
    HIGH_PASS
};

static int make_butterworth_gain(struct Buffer* buf,
                                 unsigned int order,
                                 unsigned int resol,
                                 int mode) {
    buf->size = resol;
    buf->interp = INTERP_LINEAR;
    if ((buf->data = malloc(resol * sizeof(float)))) {
        unsigned int i;
        float t;

        if (mode == LOW_PASS) {
            for (i = 0; i < resol; i++) {
                t = 10 * (float) i / (float) resol;
                buf->data[i] = 1 / sqrt(1 + pow(t, 2 * order));
            }
        } else if (mode == HIGH_PASS) {
            for (i = 0; i < resol; i++) {
                t = 10 * (float) i / (float) resol;
                buf->data[i] = 1 / sqrt(1 + pow(t, -2. * order));
            }
        }
        return 1;
    }
    buf->size = 0;
    return 0;
}

static int get_mode(struct Node* n, int* mode) {
    const char* modestr = n->inputs[MOD]->content.str;

    if (!strcmp(modestr, "lowpass")) {
        *mode = LOW_PASS;
    } else if (!strcmp(modestr, "highpass")) {
        *mode = HIGH_PASS;
    } else {
        fprintf(stderr, "Error: %s: 'mode' must be 'lowpass' or 'highpass'\n",
                        n->name);
        return 0;
    }
    return 1;
}

static int filter_process(struct Node* n) {
    struct Buffer *in, *out, gain = {0};
    struct Data* cutoffdata;
    float *win = NULL, *fftin = NULL, order;
    unsigned int winSize, stride, ok = 0;
    int mode;
    fftwf_complex* fftout = NULL;
    fftwf_plan forward = NULL, backward = NULL;

    GENERIC_CHECK_INPUTS(n, fftbp);
    if (!get_mode(n, &mode)) return 0;

    setup(n);
    in = &n->inputs[INP]->content.buf;
    out = &n->outputs[0]->content.buf;
    cutoffdata = n->inputs[COF];

    winSize = data_float(n->inputs[FTW], 0, DEFAULT_WIN_SIZE);
    order = data_float(n->inputs[ORD], 0, DEFAULT_ORDER);
    stride = winSize / 2;

    if (       (win = malloc(winSize * sizeof(float)))
            && (out->data = calloc(in->size, sizeof(float)))
            && (fftin = fftwf_malloc(winSize * sizeof(float)))
            && (fftout = fftwf_malloc((winSize / 2 + 1) * sizeof(*fftout)))
            && (forward = fftwf_plan_dft_r2c_1d(winSize, fftin, fftout, 0))
            && (backward = fftwf_plan_dft_c2r_1d(winSize, fftout, fftin, 0))
            && make_butterworth_gain(&gain, order, GAIN_RESOL, mode)) {
        int i;
        float f0;

        make_window(win, winSize);
        for (i = -stride; i < (int) in->size; i += stride) {
            f0 = data_float(cutoffdata, (float) i / (float) in->size, 0);
            load_fftin(fftin, in->data, in->size, win, winSize, i);
            fftwf_execute(forward);
            apply_filter(fftout, winSize, in->samplingRate, f0, &gain);
            fftwf_execute(backward);
            export_fftin(fftin, out->data, out->size, win, winSize, i);
        }
        ok = 1;
    }
    free(win);
    free(gain.data);
    if (forward) fftwf_destroy_plan(forward);
    if (backward) fftwf_destroy_plan(backward);
    fftwf_free(fftin);
    fftwf_free(fftout);
    return ok;
}
