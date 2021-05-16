#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sndc.h"

void node_init(struct Node* node) {
    unsigned int i;

    for (i = 0; i < MAX_INPUTS; i++) {
        node->inputs[i] = NULL;
    }
    for (i = 0; i < MAX_OUTPUTS; i++) {
        node->outputs[i] = NULL;
    }
    node->setup = NULL;
    node->process = NULL;
    node->teardown = NULL;
}

void node_free(struct Node* node) {
    if (node->teardown) {
        node->teardown(node);
    }
    free((char*)node->name);
}

void stack_init(struct Stack* stack) {
    stack->data = NULL;
    stack->nodes = NULL;
    stack->numNodes = 0;
    stack->numData = 0;
}

void stack_free(struct Stack* stack) {
    unsigned int i;

    for (i = 0; i < stack->numNodes; i++) {
        node_free(stack->nodes + i);
    }
    free(stack->nodes);
    for (i = 0; i < stack->numData; i++) {
        data_free(stack->data + i);
    }
    free(stack->data);
}

struct Node* stack_node_new_from_module(struct Stack* stack,
                                        const char* name,
                                        struct Module* module) {
    void* tmp;
    struct Node* new;
    unsigned int i;

    if (!(tmp = realloc(stack->nodes,
                        (stack->numNodes + 1) * sizeof(struct Node)))) {
        return NULL;
    }
    stack->nodes = tmp;
    new = stack->nodes + (stack->numNodes++);
    node_init(new);
    new->name = name;
    new->module = module->name;
    new->setup = module->setup;
    new->process = module->process;
    new->teardown = module->teardown;
    for (i = 0; i < MAX_OUTPUTS; i++) {
        if (module->outputNames[i]) {
            struct Data* data;

            if (!(data = stack_data_new(stack))) {
                stack->numNodes--;
                return NULL;
            }
            new->outputs[i] = data;
        }
    }
    return new;
}

struct Data* stack_data_new(struct Stack* stack) {
    void* tmp;
    struct Data* new;

    if (!(tmp = realloc(stack->data,
                        (stack->numData + 1) * sizeof(struct Data)))) {
        return NULL;
    }
    stack->data = tmp;
    new = stack->data + (stack->numData++);
    data_init(new);
    return new;
}

struct Node* stack_get_node(struct Stack* stack, const char* name) {
    unsigned int i;

    for (i = 0; i < stack->numNodes; i++) {
        if (!strcmp(stack->nodes[i].name, name)) {
            return stack->nodes + i;
        }
    }
    return NULL;
}

int stack_valid(struct Stack* stack) {
    return 1;
}

int stack_process(struct Stack* stack) {
    unsigned int i;

    for (i = 0; i < stack->numNodes; i++) {
        fprintf(stderr, "Processing %s\n", stack->nodes[i].name);
        if (!stack->nodes[i].process(stack->nodes + i)) {
            fprintf(stderr, "Error: processing failed\n");
            return 0;
        }
    }
    return 1;
}
