#include <stdlib.h>

#include <sndc.h>
#include <modules/utils.h>

static int slider_process(struct Node* n);

/* DECLARE_MODULE(slider) */
const struct Module slider = {
    "slider", "Mix 2 input buffers according to a specified gain profile",
    {
        {"input0",  DATA_BUFFER,                REQUIRED, "input #0"},
        {"input1",  DATA_BUFFER,                REQUIRED, "input #1"},
        {"slider",  DATA_FLOAT | DATA_BUFFER,   REQUIRED, "slider"},
        {"profile", DATA_BUFFER,                OPTIONAL, "mix profile"}
    },
    {
        {"out",     DATA_BUFFER,                REQUIRED, "output buffer"}
    },
    NULL,
    slider_process,
    NULL
};

enum SliderInputType {
    IN0,
    IN1,
    SLI,
    PRO,

    NUM_INPUTS
};

static int slider_process(struct Node* n) {
    struct Buffer *in0, *in1, *profile = NULL, *out, linear = {0};
    struct Data* slide;
    float lin[2] = {1, 0};
    unsigned int i;

    GENERIC_CHECK_INPUTS(n, slider);

    in0 = &n->inputs[IN0]->content.buf;
    in1 = &n->inputs[IN1]->content.buf;
    slide = n->inputs[SLI];

    if (in0->samplingRate != in1->samplingRate) {
        fprintf(stderr, "Error: %s: "
                        "input buffers have inconsistent sampling rate\n",
                        n->name);
        return 0;
    }

    if (n->inputs[PRO]) {
        profile = &n->inputs[PRO]->content.buf;
    } else {
        linear.interp = INTERP_LINEAR;
        linear.data = lin;
        linear.size = 2;
        profile = &linear;
    }

    n->outputs[0]->type = DATA_BUFFER;
    out = &n->outputs[0]->content.buf;
    out->samplingRate = in0->samplingRate;
    out->interp = in0->interp;
    out->size = in0->size >= in1->size ? in0->size : in1->size;
    if (!(out->data = calloc(out->size, sizeof(float)))) {
        fprintf(stderr, "Error: %s: can't malloc output buffer\n", n->name);
        return 0;
    }

    for (i = 0; i < in0->size; i++) {
        float t = (float) i / (float) out->size;
        out->data[i] = interp(profile, data_float(slide, t, 0)) * in0->data[i];
    }
    for (i = 0; i < in1->size; i++) {
        float t = (float) i / (float) out->size;
        out->data[i] += interp(profile, (1 - data_float(slide, t, 0)))
                        * in1->data[i];
    }
    return 1;
}
