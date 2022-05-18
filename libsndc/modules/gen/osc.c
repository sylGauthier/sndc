#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include <sndc.h>
#include <modules/utils.h>

#define OUT 0

#define DEF_FRQ 0
#define DEF_AMP 0.5
#define DEF_PAR 0.5
#define DEF_SPL 44100

#define OSC_MAX_PARAMS 6

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

        {"duration",    DATA_FLOAT,                 REQUIRED,
                        "duration of resulting signal in seconds"},

        {"sampling",    DATA_FLOAT,                 OPTIONAL,
                        "sampling rate, def 44100Hz"},

        {"interp",      DATA_STRING,                OPTIONAL,
                        "interpolation of the resulting buffer, "
                        "'step', 'linear' or 'sine'"},

        {"param0",      DATA_FLOAT | DATA_BUFFER,   OPTIONAL,
                        "wave parameter 0"},

        {"param1",      DATA_FLOAT | DATA_BUFFER,   OPTIONAL,
                        "wave parameter 1"},

        {"param2",      DATA_FLOAT | DATA_BUFFER,   OPTIONAL,
                        "wave parameter 2"},

        {"param3",      DATA_FLOAT | DATA_BUFFER,   OPTIONAL,
                        "wave parameter 3"},

        {"param4",      DATA_FLOAT | DATA_BUFFER,   OPTIONAL,
                        "wave parameter 4"},

        {"param5",      DATA_FLOAT | DATA_BUFFER,   OPTIONAL,
                        "wave parameter 5"}
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
    DUR, /* signal duration */
    SPL, /* sampling rate */
    ITP, /* interpolation to use on output */

    PR0, /* wave parameter 0 */
    PR1, /* wave parameter 1 */
    PR2, /* wave parameter 2 */
    PR3, /* wave parameter 3 */
    PR4, /* wave parameter 4 */
    PR5, /* wave parameter 5 */

    NUM_INPUTS
};

struct OscFunction {
    const char* name;
    float (*func)(float, float[]);
    unsigned int numParams;
};

static float osc_sin(float t, float params[]) {
    return sin(2. * M_PI * t);
}

static float osc_square(float t, float params[]) {
    if (t < 0.5 + params[0]) return 1.;
    return -1.;
}

static float osc_saw(float t, float params[]) {
    float thresh = 0.0001;
    float p = params[0] > 0.5 - thresh ? (0.5 - thresh) : params[0];
    p = p < -(0.5 - thresh) ? -(0.5 + thresh) : p;

    if (t < 0.5 + p) {
        return -1. + 2. * t / (0.5 - p);
    }
    return 1. - 2. * (t - 0.5 + p) / (0.5 + p);
}

static struct OscFunction functions[] = {
    { "sin",    osc_sin,    0 },
    { "square", osc_square, 1 },
    { "saw",    osc_saw,    1 }
};

static const int numFunctions = sizeof(functions) / sizeof(struct OscFunction);

static int osc_valid(struct Node* n) {
    struct Buffer* out;

    GENERIC_CHECK_INPUTS(n, osc);

    if (n->inputs[ITP] && !data_string_valid(n->inputs[ITP],
                                             interpNames,
                                             osc.inputs[ITP].name,
                                             n->name)) {
        return 0;
    }

    n->outputs[OUT]->type = DATA_BUFFER;
    out = &n->outputs[OUT]->content.buf;

    if (n->inputs[SPL]) out->samplingRate = n->inputs[SPL]->content.f;
    else out->samplingRate = 44100;

    if (!n->inputs[ITP]) out->interp = INTERP_LINEAR;
    else if ((out->interp = data_parse_interp(n->inputs[ITP])) < 0) return 0;

    if (!strcmp(n->inputs[FUN]->content.str, "input") && !n->inputs[WAV]) {
        fprintf(stderr, "Error: %s: "
                        "function 'input' requires to set 'waveform'\n",
                        n->name);
        return 0;
    }

    out->size = n->inputs[DUR]->content.f * out->samplingRate;
    out->data = NULL;
    return 1;
}

static struct OscFunction* get_fun(const char* name) {
    unsigned int i;
    for (i = 0; i < numFunctions; i++) {
        if (!strcmp(functions[i].name, name)) {
            return &functions[i];
        }
    }
    return NULL;
}

static int osc_process(struct Node* n) {
    struct Data* out = n->outputs[OUT];
    float f, s, d, t, amp, aoff, *data;
    unsigned int i, size;
    struct OscFunction* fun;

    if (!osc_valid(n)) {
        fprintf(stderr, "Error: %s: invalid inputs\n", n->name);
        return 0;
    }
    if (       !(fun = get_fun(n->inputs[FUN]->content.str))
            && strcmp(n->inputs[FUN]->content.str, "input")) {
        fprintf(stderr, "Error: %s: invalid function: %s\n",
                        n->name,
                        n->inputs[FUN]->content.str);
        return 0;
    }

    d = n->inputs[DUR]->content.f;
    if (n->inputs[SPL]) {
        s = n->inputs[SPL]->content.f;
    } else {
        s = DEF_SPL;
    }
    size = d * s;
    if (!(data = malloc(size * sizeof(float)))) {
        return 0;
    }

    t = data_float(n->inputs[POF], 0, 0);
    aoff = data_float(n->inputs[AOF], 0, 0);
    if (!fun) {
        for (i = 0; i < size; i++) {
            float x = (float) i / (float) size;

            f = data_float(n->inputs[FRQ], x, DEF_FRQ);
            amp = data_float(n->inputs[AMP], x, DEF_AMP);

            data[i] = amp * interp(&n->inputs[WAV]->content.buf, t) + aoff;

            t += f / s;
            if (t > 1) t -= 1;
        }
    } else {
        float (*oscf)(float, float[]) = fun->func;
        for (i = 0; i < size; i++) {
            float x = (float) i / (float) size;
            float params[OSC_MAX_PARAMS];
            int j;

            f = data_float(n->inputs[FRQ], x, DEF_FRQ);
            amp = data_float(n->inputs[AMP], x, DEF_AMP);
            for (j = 0; j < fun->numParams; j++) {
                params[j] = data_float(n->inputs[PR0 + j], x, 0);
            }

            data[i] = amp * oscf(t, params) + aoff;

            t += f / s;
            if (t > 1) t -= 1;
        }
    }

    out->content.buf.data = data;
    out->content.buf.size = d * s;
    out->ready = 1;
    return 1;
}
