#include <math.h>
#include <string.h>

#include "utils.h"

int data_valid(struct Data* data,
               const struct DataDesc* desc,
               const char* ctx) {
    if (!data && !desc->req) return 1;
    if (!data) {
        fprintf(stderr, "Error: %s: %s is required\n", ctx, desc->name);
        return 0;
    }
    if (!(data->type & desc->type)) {
        fprintf(stderr, "Error: %s: %s expects type %d but got %d\n",
                        ctx, desc->name, desc->type, data->type);
        return 0;
    }
    if (data->type == DATA_STRING && !data->content.str) {
        fprintf(stderr, "Error: %s: %s has a NULL string\n",
                        ctx, desc->name);
        return 0;
    }
    return 1;
}

int data_parse_interp(struct Data* data) {
    if (!data) return -1;
    if (data->type != DATA_STRING) return -1;
    if (!data->content.str) return -1;
    if (!strcmp(data->content.str, "step")) return INTERP_STEP;
    if (!strcmp(data->content.str, "linear")) return INTERP_LINEAR;
    if (!strcmp(data->content.str, "sine")) return INTERP_SINE;
    return -1;
}

int data_string_valid(struct Data* data, const char* strings[],
                      const char* inputName, const char* nodeName) {
    unsigned int i;

    if (data->type != DATA_STRING || !data->content.str) {
        fprintf(stderr, "Error: %s: %s must be a string\n",
                        nodeName, inputName);
        return 0;
    }
    for (i = 0; strings[i]; i++) {
        if (!strcmp(data->content.str, strings[i])) return 1;
    }
    fprintf(stderr, "Error: %s: %s must be one of:\n", nodeName, inputName);
    for (i = 0; strings[i]; i++) fprintf(stderr, "%s ", strings[i]);
    fprintf(stderr, "\n");
    return 0;
}

float data_float(struct Data* data, float s, float def) {
    if (!data) return def;
    switch (data->type) {
        case DATA_FLOAT:
            return data->content.f;
        case DATA_BUFFER:
            return interp(&data->content.buf, s);
        default:
            return 0;
    }
}

float interp(struct Buffer* buf, float t) {
    float a = t * buf->size;
    float f, r;
    unsigned int i1, i2;

    f = floor(a);
    r = a - f;
    i1 = (unsigned int) f % buf->size;;
    i2 = (i1 + 1) % buf->size;
    switch (buf->interp) {
        case INTERP_STEP:
            return buf->data[i1];
        case INTERP_LINEAR:
            return buf->data[i1] * (1 - r) + buf->data[i2] * r;
        case INTERP_SINE:
            return (buf->data[i1] - buf->data[i2]) / 2. * cos(M_PI * r) +
                   (buf->data[i1] + buf->data[i2]) / 2.;
    }
    return 0;
}

float interpf(int type, float a, float b, float t) {
    switch (type) {
        case INTERP_STEP:
            return a;
        case INTERP_LINEAR:
            return a * (1 - t) + b * t;
        case INTERP_SINE:
            return (a - b) / 2. * cos(M_PI * t) + (a + b) / 2.;
    }
    return 0;
}

float convol(struct Buffer* buf,
             struct Buffer* fun,
             unsigned int maskWidth,
             unsigned int pos) {
    unsigned int i1, i2, i;
    float res = 0, f, sum = 0;

    if (pos < maskWidth / 2) i1 = 0;
    else i1 = pos - maskWidth / 2;

    if (pos + maskWidth / 2 >= buf->size) i2 = buf->size - 1;
    else i2 = pos + maskWidth / 2;

    for (i = i1; i <= i2; i++) {
        float t = ((float) i - (float) pos) / (float) maskWidth + 0.5;
        f = interp(fun, t);
        res += buf->data[i] * f;
        sum += f;
    }
    res /= sum;
    return res;
}
