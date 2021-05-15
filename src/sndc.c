#include <stdio.h>

#include "sndc.h"

int main(int argc, char** argv) {
    struct Module* m;
    struct Node* n;
    struct Stack s;

    if (!module_load_all()) return 1;
    stack_init(&s);

    if (!(m = module_find("sin"))) return 1;
    if (!(n = stack_node_new_from_module(&s, "base", m))) return 1;

    n->inputs[0] = stack_data_new(&s);
    n->inputs[0]->type = DATA_FLOAT;
    n->inputs[0]->content.f = 440;

    n->inputs[1] = stack_data_new(&s);
    n->inputs[1]->type = DATA_FLOAT;
    n->inputs[1]->content.f = 5;

    n->inputs[2] = stack_data_new(&s);
    n->inputs[2]->type = DATA_FLOAT;
    n->inputs[2]->content.f = 44100;

    n->outputs[0] = stack_data_new(&s);
    n->outputs[0]->type = DATA_BUFFER;


    if (stack_valid(&s)) {
        stack_process(&s);
    }
    fwrite(n->outputs[0]->content.buf.data, sizeof(float),n->outputs[0]->content.buf.size, stdout);
    stack_free(&s);

    return 0;
}
