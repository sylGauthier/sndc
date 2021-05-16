#ifndef M_UTILS_H
#define M_UTILS_H

#include "../sndc.h"

#define REQUIRED 1
#define OPTIONAL 0

int data_valid(struct Data* data, int type, char required,
               const char* ctx, const char* name);

float interp(float* buf, unsigned int size, float t);

#endif

