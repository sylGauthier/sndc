#include <stdio.h>

#ifndef SDNC_H
#define SDNC_H

#define MAX_INPUTS          16
#define MAX_OUTPUTS         16

#define MAX_SNDC_PATH       16
#define MAX_PATH_LENGTH     128
#define MAX_MOD_NAME_LEN    16

extern char sndcPath[MAX_SNDC_PATH][MAX_PATH_LENGTH];

enum InterpType {
    INTERP_STEP,
    INTERP_LINEAR,
    INTERP_SINE
};

struct Buffer {
    float* data;
    unsigned int size;
    unsigned int samplingRate;
    enum InterpType interp;
};

struct Data {
    enum DataType {
        DATA_UNKNOWN    = 0,
        DATA_BUFFER     = 1 << 0,
        DATA_FLOAT      = 1 << 1,
        DATA_STRING     = 1 << 2,
        DATA_NODE       = 1 << 3
    } type;
    union DataContent {
        struct Buffer buf;
        float f;
        char* str;
        struct Node* node;
    } content;
    char ready;
};

void data_init(struct Data* data);
void data_free(struct Data* data);


struct Module;

struct Node {
    struct Data* inputs[MAX_INPUTS];
    struct Data* outputs[MAX_OUTPUTS];

    int (*setup)(struct Node*);
    int (*process)(struct Node*);
    int (*teardown)(struct Node* node);

    const char* name;
    const char* path;
    char isSetup;
    const struct Module* module;
    void* data;
};

void node_init(struct Node* node);
void node_free(struct Node* node);
void node_flush_output(struct Node* node);


struct DataDesc {
    const char* name;
    int type;
    enum DataReq {
        OPTIONAL = 0,
        REQUIRED
    } req;
    const char* description;
};

struct SNDCFile;

struct Module {
    const char* name;
    const char* desc;
    struct DataDesc inputs[MAX_INPUTS];
    struct DataDesc outputs[MAX_OUTPUTS];

    int (*setup)(struct Node* node);
    int (*process)(struct Node* node);
    int (*teardown)(struct Node* node);

    struct SNDCFile* file;
};

extern const struct Module* modules[];

const struct Module* module_find(const char* name);
int module_get_input_slot(const struct Module* module, const char* name);
int module_get_output_slot(const struct Module* module, const char* name);
int module_import(struct Module* module, const char* name, const char* file);
void module_free_import(struct Module* module);


struct Stack {
    struct Module** imports;
    struct Data** data;
    struct Node** nodes;
    unsigned int numNodes, numData, numImports;

    char* path;
    char verbose;
};

void stack_init(struct Stack* stack);
void stack_free(struct Stack* stack);

struct Node* stack_node_new(struct Stack* stack, const char* name);
int stack_node_set_module(struct Stack* stack,
                          struct Node* node,
                          const struct Module* module);

struct Data* stack_data_new(struct Stack* stack);
struct Module* stack_import_new(struct Stack* stack);
struct Node* stack_get_node(struct Stack* stack, const char* name);
int stack_process(struct Stack* stack);

int stack_load(struct Stack* stack, struct SNDCFile* file);

#endif
