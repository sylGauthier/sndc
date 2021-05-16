#include <math.h>

#include "utils.h"

int data_valid(struct Data* data, int type, char required,
               const char* ctx, const char* name) {
    if (!data && !required) return 1;
    if (!data) {
        fprintf(stderr, "Error: %s: %s is required\n", ctx, name);
        return 0;
    }
    if (!(data->type & type)) {
        fprintf(stderr, "Error: %s: %s expects type %d but got %d\n",
                        ctx, name, type, data->type);
        return 0;
    }
    if (data->type == DATA_STRING && !data->content.str) {
        fprintf(stderr, "Error: %s: %s has a NULL string\n",
                        ctx, name);
        return 0;
    }
    return 1;
}

float interp(float* buf, unsigned int size, float t) {
    float a = t * size;
    float f, r;
    unsigned int i1, i2;

    f = floor(a);
    r = a - f;
    i1 = f;
    i2 = (i1 + 1) % size;
    return buf[i1] * (1 - r) + buf[i2] * r;
}
