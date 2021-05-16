#include <stdio.h>

#ifndef SDNC_H
#define SDNC_H

#define MAX_INPUTS  16
#define MAX_OUTPUTS 16
#define MAX_MODULES 16

struct Buffer {
    float* data;
    unsigned int size;
    unsigned int samplingRate;
};

struct Data {
    enum DataType {
        DATA_BUFFER     = 1 << 0,
        DATA_FLOAT      = 1 << 1,
        DATA_STRING     = 1 << 2
    } type;
    union DataContent {
        struct Buffer buf;
        float f;
        char* str;
    } content;
    char ready;
};

void data_init(struct Data* data);
void data_free(struct Data* data);


struct Node {
    struct Data* inputs[MAX_INPUTS];
    struct Data* outputs[MAX_OUTPUTS];

    int (*setup)(struct Node*);
    int (*process)(struct Node*);
    int (*teardown)(struct Node* node);

    const char* name;
    const char* module;
};

void node_init(struct Node* node);
void node_free(struct Node* node);


struct Module {
    const char* name;
    const char* inputNames[MAX_INPUTS];
    const char* outputNames[MAX_INPUTS];

    int (*setup)(struct Node* node);
    int (*process)(struct Node* node);
    int (*teardown)(struct Node* node);
};

extern struct Module modules[MAX_MODULES];
extern unsigned int numModules;

int module_load_all();
struct Module* module_find(const char* name);
int module_get_input_slot(struct Module* module, const char* name);
int module_get_output_slot(struct Module* module, const char* name);


struct Stack {
    struct Data* data;
    struct Node* nodes;
    unsigned int numNodes, numData;
};

void stack_init(struct Stack* stack);
void stack_free(struct Stack* stack);
struct Node* stack_node_new_from_module(struct Stack* stack,
                                        const char* name,
                                        struct Module* module);
struct Data* stack_data_new(struct Stack* stack);
struct Node* stack_get_node(struct Stack* stack, const char* name);
int stack_valid(struct Stack* stack);
int stack_process(struct Stack* stack);
int stack_load(struct Stack* stack, FILE* in);

#endif
