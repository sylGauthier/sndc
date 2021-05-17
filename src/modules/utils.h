#ifndef M_UTILS_H
#define M_UTILS_H

#include "../sndc.h"

#define REQUIRED 1
#define OPTIONAL 0
#define M_PI 3.14159265358979

int data_valid(struct Data* data, const struct DataDesc* desc, const char* ctx);

float interp(float* buf, unsigned int size, float t);

#endif
