#include <stdlib.h>
#include <string.h>

#include <sndc.h>
#include <modules/utils.h>

#define NUM_SAMPLES 12

static int layout_process(struct Node* n);

/* DECLARE_MODULE(layout) */
const struct Module layout = {
    "layout", "sequencer", "A generic layout helper to mix layers together",
    {
        {"bpm",         DATA_FLOAT,     REQUIRED,
                        "tempo in beats per minute"},
        {"bpb",         DATA_FLOAT,     OPTIONAL,
                        "number of beats per bar, def 4"},
        {"divs",        DATA_FLOAT,     OPTIONAL,
                        "number of subdivision per beat, def 4"},
        {"file",        DATA_STRING,    REQUIRED,
                        "sequence layout file"},

        {"sample0",     DATA_BUFFER,    REQUIRED, "sample #0"},
        {"sample1",     DATA_BUFFER,    OPTIONAL, "sample #1"},
        {"sample2",     DATA_BUFFER,    OPTIONAL, "sample #2"},
        {"sample3",     DATA_BUFFER,    OPTIONAL, "sample #3"},
        {"sample4",     DATA_BUFFER,    OPTIONAL, "sample #4"},
        {"sample5",     DATA_BUFFER,    OPTIONAL, "sample #5"},
        {"sample6",     DATA_BUFFER,    OPTIONAL, "sample #6"},
        {"sample7",     DATA_BUFFER,    OPTIONAL, "sample #7"},
        {"sample8",     DATA_BUFFER,    OPTIONAL, "sample #8"},
        {"sample9",     DATA_BUFFER,    OPTIONAL, "sample #9"},
        {"sample10",    DATA_BUFFER,    OPTIONAL, "sample #10"},
        {"sample11",    DATA_BUFFER,    OPTIONAL, "sample #11"},
    },
    {
        {"out",         DATA_BUFFER,    REQUIRED, "output, full sequence"}
    },
    NULL,
    layout_process,
    NULL
};

enum LayoutInput {
    BPM = 0,
    BPB,
    DIV,
    SFL,

    S00,
    S01,
    S02,
    S03,
    S04,
    S05,
    S06,
    S07,
    S08,
    S09,
    S10,
    S11,
    NUM_INPUTS
};

struct Layer {
    unsigned int start;
    unsigned int end;
};

struct LayerArray {
    struct Layer* layers;
    unsigned int numLayers;
    unsigned int scope;
};

struct Context {
    unsigned int sampling;
    unsigned int bpm;
    unsigned int bpb;
    unsigned int divs;
};

enum LToken {
    LTK_UNKOWN,
    LTK_LAYER_ID,
    LTK_OSB,
    LTK_MINUS,
    LTK_EQUAL,
    LTK_NEWLINE,
    LTK_EOF
};

static int next_token(FILE* f, int* val) {
    char num[10] = {0};
    int i, cur;

    while ((cur = fgetc(f)) != EOF) {
        switch (cur) {
            case 's':
                i = 0;
                while ((cur = fgetc(f)) != EOF) {
                    if (cur >= '0' && cur <= '9' && i + 1 < sizeof(num)) {
                        num[i++] = cur;
                    } else if (cur == ':' && i > 0) {
                        if (val) *val = strtol(num, NULL, 10);
                        return LTK_LAYER_ID;
                    } else {
                        fprintf(stderr, "Error: layout: unkown token: %c\n",
                                cur);
                        return LTK_UNKOWN;
                    }
                }
                break;
            case '[': return LTK_OSB;
            case '-': return LTK_MINUS;
            case '=': return LTK_EQUAL;
            case '\n': return LTK_NEWLINE;
            case ' ':
            case '\t':
                continue;
            default:
                fprintf(stderr, "Error: layout: unkown token: %c\n", cur);
                return LTK_UNKOWN;
        }
    }
    return LTK_EOF;
}

static struct Layer* new_layer(struct LayerArray* layers) {
    struct Layer* tmp;

    if (!(tmp = realloc(layers->layers,
                        (layers->numLayers + 1) * sizeof(struct Layer)))) {
        fprintf(stderr, "Error: layout: new_layer: can't realloc layers\n");
    } else {
        struct Layer* new = &tmp[layers->numLayers];

        layers->layers = tmp;
        layers->numLayers++;
        new->start = 0;
        new->end = 0;
        return new;
    }
    return NULL;
}

static int parse_layers(struct Context* ctx,
                        FILE* f,
                        struct LayerArray* layers,
                        int* err) {
    int layerID, tk;

    *err = 1;
    if ((tk = next_token(f, &layerID)) != LTK_LAYER_ID) {
        if (tk == LTK_EOF) {
            *err = 0;
            return 0;
        } else if (tk == LTK_NEWLINE) {
            *err = 0;
            return 1;
        } else {
            fprintf(stderr, "Error: layout: unexpected token: %d\n", tk);
        }
    } else if (layerID < 0 || layerID >= NUM_SAMPLES) {
        fprintf(stderr, "Error: layout: invalid sample id: %d\n", layerID);
    } else {
        struct LayerArray* array = layers + layerID;
        struct Layer* cur = NULL;
        int token;
        unsigned int pos = array->scope;
        unsigned int incr = 60 * ctx->sampling / ctx->bpm;

        while ((token = next_token(f, NULL))) {
            switch (token) {
                case LTK_OSB:
                    if (cur) cur->end = pos;
                    if (!(cur = new_layer(array))) return 0;
                    cur->start = pos;
                    pos += incr;
                    break;
                case LTK_MINUS:
                    if (cur) {
                        cur->end = pos;
                        cur = NULL;
                    }
                    pos += incr;
                    break;
                case LTK_EQUAL:
                    if (!cur) {
                        fprintf(stderr, "Error: layout: layout file: "
                                        "'=' mut be preceded by '=' or '['\n");
                        return 0;
                    }
                    pos += incr;
                    break;
                case LTK_NEWLINE:
                    if (cur) cur->end = pos;
                    array->scope = pos;
                    return 1;
                case LTK_EOF:
                    if (cur) cur->end = pos;
                    array->scope = pos;
                    *err = 0;
                    return 0;
                default:
                    return 0;
            }
        }
    }
    return 0;
}

static int load_layers(struct Context* ctx,
                       struct LayerArray* layers,
                       const char* path,
                       const char* file) {
    FILE* f = NULL;
    char* fullpath = NULL;
    int ok = 0;

    if (!(fullpath = malloc(strlen(path) + strlen(file) + 1))) {
        fprintf(stderr, "Error: load_layers: can't allocate fullpath\n");
    } else if (!strcpy(fullpath, path)
            || !strcpy(fullpath + strlen(path), file)
            || !(f = fopen(fullpath, "r"))) {
        fprintf(stderr, "Error: load_layers: can't open file: %s\n", fullpath);
    } else {
        int err;
        while (parse_layers(ctx, f, layers, &err));
        if (!err) ok = 1;
    }
    if (f) fclose(f);
    free(fullpath);
    return ok;
}

static int layout_process(struct Node* n) {
    struct LayerArray layers[NUM_SAMPLES] = {0};
    struct Context ctx = {0};
    struct Buffer* out;
    int ok = 0, i;
    unsigned int maxsize;

    GENERIC_CHECK_INPUTS(n, layout);
    n->outputs[0]->type = DATA_BUFFER;
    out = &n->outputs[0]->content.buf;

    ctx.bpm = data_float(n->inputs[BPM], 0, 120);
    ctx.sampling = n->inputs[S00]->content.buf.samplingRate;

    for (i = 0; i < NUM_SAMPLES; i++) {
        if (   n->inputs[S00 + i]
            && n->inputs[S00 + i]->content.buf.samplingRate != ctx.sampling) {
            fprintf(stderr, "Error: layout: "
                            "inputs have inconsistent sampling rate\n");
            return 0;
        }
    }
    ok = load_layers(&ctx, layers, n->path, n->inputs[SFL]->content.str);
    if (!ok) goto exit;

    maxsize = 0;
    for (i = 0; i < NUM_SAMPLES; i++) {
        int j;
        for (j = 0; j < layers[i].numLayers; j++) {
            maxsize = layers[i].layers[j].end > maxsize ?
                      layers[i].layers[j].end : maxsize;
        }
    }
    if ((out->data = calloc(maxsize, sizeof(float)))) {
        int j;

        out->size = maxsize;
        out->samplingRate = ctx.sampling;
        for (i = 0; i < NUM_SAMPLES; i++) {
            struct Buffer* in;
            if (n->inputs[S00 + i]) {
                in = &n->inputs[S00 + i]->content.buf;
            } else {
                if (layers[i].numLayers) {
                    fprintf(stderr, "Warning: layout: "
                                    "sample #%d is empty but has a sequence "
                                    "in '%s'\n",
                                    i, n->inputs[SFL]->content.str);
                }
                continue;
            }
            for (j = 0; j < layers[i].numLayers; j++) {
                unsigned int k, min;
                struct Layer* l = layers[i].layers + j;
                min = in->size < l->end - l->start
                    ? in->size : l->end - l->start;
                for (k = 0; k < min; k++) {
                    out->data[l->start + k] += in->data[k];
                }
            }
        }
    } else {
        fprintf(stderr, "Error: layout: can't allocate output data\n");
        ok = 0;
    }

exit:
    for (i = 0; i < NUM_SAMPLES; i++) {
        free(layers[i].layers);
    }
    return ok;
}
