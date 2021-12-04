#ifndef M_UTILS_H
#define M_UTILS_H

#include "../sndc.h"

#define M_PI 3.14159265358979

#define GENERIC_CHECK_INPUTS(n, m) \
{ \
    unsigned int i; \
    if (NUM_INPUTS > MAX_INPUTS) { \
        fprintf(stderr, \
                "Error: %s: NUM_INPUTS (%d) exceeds MAX_INPUTS (%d)\n", \
                (m).name, NUM_INPUTS, MAX_INPUTS); \
        return 0; \
    } \
    for (i = 0; i < NUM_INPUTS; i++) { \
        if (!data_valid((n)->inputs[i], (m).inputs + i, (n)->name)) { \
            return 0; \
        } \
    } \
}

extern const char* interpNames[];

int data_valid(struct Data* data, const struct DataDesc* desc, const char* ctx);
float data_float(struct Data* data, float s, float def);
int data_parse_interp(struct Data* data);
int data_which_string(struct Data* data, const char* strings[]);
int data_string_valid(struct Data* data, const char* strings[],
                      const char* inputName, const char* nodeName);

float interp(struct Buffer* buf, float t);
float interpf(int type, float a, float b, float t);
float convol(struct Buffer* buf,
             struct Buffer* fun,
             unsigned int maskWidth,
             unsigned int pos);
void addbuf(float* dest, float* src, unsigned int size);

#endif
