#include <stdlib.h>
#include <string.h>

#include <sndc.h>
#include <modules/utils.h>

static int binop_process(struct Node* n);

/* DECLARE_MODULE(binop) */
const struct Module binop = {
    "binop", "Binary operation between two buffers or numbers",
    {
        {"input0",      DATA_BUFFER | DATA_FLOAT,   REQUIRED, "input #0"},
        {"input1",      DATA_BUFFER | DATA_FLOAT,   REQUIRED, "input #1"},

        {"operator",    DATA_STRING,                REQUIRED,
                        "operator: 'add', 'sub', 'mul', 'div', 'min', 'max'"},
    },
    {
        {"out",     DATA_BUFFER,                REQUIRED,
                    "result, input0 'operator' input1"}
    },
    NULL,
    binop_process,
    NULL
};

enum BinopInput {
    IN0,
    IN1,
    OPE,
    NUM_INPUTS
};

enum BinopType {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MIN,
    OP_MAX
};

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static int binop_process(struct Node* n) {
    struct Data *in0, *in1, *out;
    unsigned int i;
    int op;
    const char* opstr;

    GENERIC_CHECK_INPUTS(n, binop);

    in0 = n->inputs[IN0];
    in1 = n->inputs[IN1];
    out = n->outputs[0];
    opstr = n->inputs[OPE]->content.str;

    if (!strcmp(opstr, "add")) {
        op = OP_ADD;
    } else if (!strcmp(opstr, "sub")) {
        op = OP_SUB;
    } else if (!strcmp(opstr, "mul")) {
        op = OP_MUL;
    } else if (!strcmp(opstr, "div")) {
        op = OP_DIV;
    } else if (!strcmp(opstr, "min")) {
        op = OP_MIN;
    } else if (!strcmp(opstr, "max")) {
        op = OP_MAX;
    } else {
        fprintf(stderr, "Error: %s: invalid op: %s\n", n->name, opstr);
        return 0;
    }

    if (in0->type == DATA_FLOAT) {
        if (in1->type != DATA_FLOAT) {
            fprintf(stderr, "Error: %s: "
                    "input1 can't be a buffer if input0 is a float\n",
                    n->name);
            return 0;
        }
        switch (op) {
            case OP_ADD:
                out->content.f = in0->content.f + in1->content.f;
                break;
            case OP_SUB:
                out->content.f = in0->content.f - in1->content.f;
                break;
            case OP_MUL:
                out->content.f = in0->content.f * in1->content.f;
                break;
            case OP_DIV:
                out->content.f = in0->content.f / in1->content.f;
                break;
            case OP_MIN:
                out->content.f = MIN(in0->content.f, in1->content.f);
                break;
            case OP_MAX:
                out->content.f = MAX(in0->content.f, in1->content.f);
                break;
        }
        out->type = DATA_FLOAT;
        return 1;
    }
    memcpy(out, in0, sizeof(*out));
    if (!(out->content.buf.data = malloc(out->content.buf.size
                                         * sizeof(float)))) {
        return 0;
    }

    switch (op) {
        case OP_ADD:
            for (i = 0; i < out->content.buf.size; i++) {
                float t = (float) i / (float) out->content.buf.size;

                out->content.buf.data[i] = in0->content.buf.data[i]
                                         + data_float(in1, t, 0);
            }
            break;
        case OP_SUB:
            for (i = 0; i < out->content.buf.size; i++) {
                float t = (float) i / (float) out->content.buf.size;

                out->content.buf.data[i] = in0->content.buf.data[i]
                                         - data_float(in1, t, 0);
            }
            break;
        case OP_MUL:
            for (i = 0; i < out->content.buf.size; i++) {
                float t = (float) i / (float) out->content.buf.size;

                out->content.buf.data[i] = in0->content.buf.data[i]
                                         * data_float(in1, t, 0);
            }
            break;
        case OP_DIV:
            for (i = 0; i < out->content.buf.size; i++) {
                float t = (float) i / (float) out->content.buf.size;

                out->content.buf.data[i] = in0->content.buf.data[i]
                                         / data_float(in1, t, 0);
            }
            break;
        case OP_MIN:
            for (i = 0; i < out->content.buf.size; i++) {
                float t = (float) i / (float) out->content.buf.size;

                out->content.buf.data[i] = MIN(in0->content.buf.data[i],
                                               data_float(in1, t, 0));
            }
            break;
        case OP_MAX:
            for (i = 0; i < out->content.buf.size; i++) {
                float t = (float) i / (float) out->content.buf.size;

                out->content.buf.data[i] = MAX(in0->content.buf.data[i],
                                               data_float(in1, t, 0));
            }
            break;
    }
    return 1;
}
