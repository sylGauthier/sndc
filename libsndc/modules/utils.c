#include <math.h>
#include <string.h>

#include "utils.h"

const char* interpNames[] = {
    "step",
    "linear",
    "sine",
    NULL
};

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

int data_which_string(struct Data* data, const char* strings[]) {
    unsigned int i;

    for (i = 0; strings[i]; i++) {
        if (!strcmp(data->content.str, strings[i])) return i;
    }
    return -1;
}

int data_string_valid(struct Data* data, const char* strings[],
                      const char* inputName, const char* nodeName) {
    unsigned int i;

    if (!data || data->type != DATA_STRING || !data->content.str) {
        fprintf(stderr, "Error: %s: %s must be a string\n",
                        nodeName, inputName);
        return 0;
    }

    if (data_which_string(data, strings) >= 0) {
        return 1;
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
    float a = t * (buf->size - 1);
    float f, r;
    unsigned int i1, i2;

    if (t <= 0) return buf->data[0];
    if (t >= 1) return buf->data[buf->size - 1];

    f = floor(a);
    r = a - f;
    i1 = (unsigned int) f;
    i2 = (i1 + 1);
    if (i1 >= buf->size || i2 >= buf->size) {
        return buf->data[buf->size - 1];
    }
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

void addbuf(float* dest, float* src, unsigned int size) {
    unsigned int i;

    for (i = 0; i < size; i++) {
        dest[i] += src[i];
    }
}

/* A4 = 440 Hz */
static float freqs[12] = {
    4186.01, /* Do   | C8  */
    4434.92, /* Do#  | C#8 */
    4698.63, /* Ré   | D8  */
    4978.03, /* Ré#  | D#8 */
    5274.04, /* Mi   | E8  */
    5587.65, /* Fa   | F8  */
    5919.91, /* Fa#  | F#8 */
    6271.93, /* Sol  | G8  */
    6644.88, /* Sol# | G#8 */
    7040.00, /* La   | A8  */
    7458.62, /* La#  | A#8 */
    7902.13, /* Si   | B8  */
};

int note_to_freq(const char* note, float* freq) {
    int offset = 0, octave = 0, pitchID;
    const char* cur = note;

    if (strlen(note) < 2) return -1;
    switch (*cur) {
        case 'A': offset = 9; break;
        case 'B': offset = 11; break;
        case 'C': offset = 0; break;
        case 'D': offset = 2; break;
        case 'E': offset = 4; break;
        case 'F': offset = 5; break;
        case 'G': offset = 7; break;
        default: return -1;
    }
    cur++;
    if (*cur == '#' || *cur == 'b') {
        if (strlen(cur) < 2) return -1;
        offset += (*cur == '#' ? 1 : -1);
        cur++;
    }
    if (*cur < '0' || *cur > '9') return -1;
    octave = (*cur - '0');

    if (offset < 0) {
        offset += 12;
        octave -= 1;
    } else if (offset > 11) {
        offset -= 12;
        octave += 1;
    }
    if (octave < 0) octave = 0;
    if (octave > 8) octave = 8;
    *freq = freqs[offset];
    pitchID = octave * 12 + offset;
    for (; octave < 8; octave++) *freq /= 2.;
    return pitchID;
}
