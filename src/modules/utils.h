#ifndef M_UTILS_H
#define M_UTILS_H

#include "../sndc.h"

#define REQUIRED 1
#define OPTIONAL 0
#define M_PI 3.14159265358979

int data_valid(struct Data* data, const struct DataDesc* desc, const char* ctx);
float data_float(struct Data* data, float s, float def);
int data_parse_interp(struct Data* data);
int data_string_valid(struct Data* data, const char* strings[],
                      const char* inputName, const char* nodeName);

float interp(struct Buffer* buf, float t);

#endif
