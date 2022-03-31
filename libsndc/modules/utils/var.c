#include <stdio.h>

#include <sndc.h>
#include <modules/utils.h>

static int var_process(struct Node* n);

/* DECLARE_MODULE(var) */
const struct Module var = {
    "var", "Variable, copies its input value to its output value",
    {
        {"value",   DATA_FLOAT | DATA_STRING,
                    REQUIRED,
                    "value of the variable (string or float)"},
        {"min",     DATA_FLOAT,
                    OPTIONAL,
                    "min value (if value is a float)"},
        {"max",     DATA_FLOAT,
                    OPTIONAL,
                    "max value (if value is a float)"},
    },
    {
        {"value",   DATA_FLOAT | DATA_STRING,
                    REQUIRED,
                    "output value equal to input value"}
    },
    NULL,
    var_process,
    NULL
};

enum VarInputType {
    VAL,
    MIN,
    MAX,

    NUM_INPUTS
};

int var_process(struct Node* n) {
    float input;
    GENERIC_CHECK_INPUTS(n, var);
    
    if (n->inputs[VAL]->type == DATA_FLOAT) {
        input = n->inputs[VAL]->content.f;
        if (       n->inputs[MIN]
                && input < n->inputs[MIN]->content.f) {
            fprintf(stderr, "Warning: node %s: "
                            "input value (%f) is below min (%f), clamping...\n",
                            n->name,
                            input,
                            n->inputs[MIN]->content.f);
            input = n->inputs[MIN]->content.f;
        }
        if (       n->inputs[MAX]
                && input > n->inputs[MAX]->content.f) {
            fprintf(stderr, "Warning: node %s: "
                            "input value (%f) is above max (%f), clamping...\n",
                            n->name,
                            input,
                            n->inputs[MAX]->content.f);
            input = n->inputs[MAX]->content.f;
        }
        n->outputs[0]->type = DATA_FLOAT;
        n->outputs[0]->content.f = input;
        return 1;
    }
    n->outputs[0]->type = DATA_STRING;
    if (!(n->outputs[0]->content.str = str_cpy(n->inputs[VAL]->content.str))) {
        return 0;
    }
    return 1;
}
