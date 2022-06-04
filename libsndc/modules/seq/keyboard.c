#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sndc.h>
#include <modules/utils.h>

static int keyboard_setup(struct Node* n);
static int keyboard_process(struct Node* n);
static int keyboard_teardown(struct Node* n);

/* DECLARE_MODULE(keyboard) */
const struct Module keyboard = {
    "keyboard", "sequencer", "Melodic sequencer for a given intrument node",
    {
        {"instrument",  DATA_STRING,    REQUIRED,
                        "the instrument node, "
                        "a SNDC file exporting at least the following inputs: "
                        "frequency, velocity, sustain"},

        {"bpm",         DATA_FLOAT,     REQUIRED,
                        "tempo in beats per minute"},

        {"divs",        DATA_FLOAT,     OPTIONAL,
                        "number of subdivisions in a beat"},

        {"file",        DATA_STRING,    REQUIRED,
                        "input melody file, see manual for format"},
    },
    {
        {"out",         DATA_BUFFER,    REQUIRED,
                        "output buffer, full sequence, sampled at 44100Hz"}
    },
    keyboard_setup,
    keyboard_process,
    keyboard_teardown
};

enum KeyboardInputType {
    INS,
    BPM,
    DIV,
    SEQ,

    NUM_INPUTS
};

static int keyboard_setup(struct Node* n) {
    const char* instPath;
    char* fullInstPath = NULL;
    struct Module* mod = NULL;
    struct Node* instNode = NULL;
    struct Data* data = NULL;
    int mi = 0;

    if (!n->inputs[INS] || n->inputs[INS]->type != DATA_STRING) {
        fprintf(stderr, "Error: %s:"
                        "instrument is required and must be of type STRING\n",
                        n->name);
        return 0;
    }

    instPath = n->inputs[INS]->content.str;

    if (       !(mod = malloc(sizeof(*mod)))
            || !(instNode = malloc(sizeof(*instNode)))
            || !(data = malloc(4 * sizeof(*data)))
            || !(fullInstPath = malloc(   strlen(n->path)
                                        + strlen(instPath) + 1))) {
        fprintf(stderr, "Error: %s: malloc failed\n", n->name);
        goto exit_err;
    }
    node_init(instNode);
    data_init(data);
    data_init(data + 1);
    data_init(data + 2);
    data_init(data + 3);

    strcpy(fullInstPath, n->path);
    strcpy(fullInstPath + strlen(n->path), instPath);
    fprintf(stderr, "DEBUG: opening instrument in %s\n", fullInstPath);
    if (!(mi = module_import(mod, "inst", fullInstPath))) {
        fprintf(stderr, "Error: %s: instrument import failed\n", n->name);
        goto exit_err;
    }
    free(fullInstPath);

    if (       module_get_input_slot(mod, "frequency") < 0
            || module_get_input_slot(mod, "velocity") < 0
            || module_get_input_slot(mod, "sustain") < 0
            || module_get_output_slot(mod, "out") < 0) {
        fprintf(stderr, "Error: %s: instrument must have inputs:\n"
                        "    frequency [FLOAT]\n"
                        "    velocity [FLOAT]\n"
                        "    sustain [FLOAT]\n"
                        "and outputs:\n"
                        "    out [BUFFER]\n",
                        n->name);
        goto exit_err;
    }

    instNode->module = mod;
    if (!(instNode->name = str_cpy("inst"))) {
        fprintf(stderr, "Error: %s: str_cpy failed\n", n->name);
        goto exit_err;
    }
    instNode->setup = mod->setup;
    instNode->process = mod->process;
    instNode->teardown = mod->teardown;

    instNode->inputs[module_get_input_slot(mod, "frequency")] = data;
    instNode->inputs[module_get_input_slot(mod, "velocity")] = data + 1;
    instNode->inputs[module_get_input_slot(mod, "sustain")] = data + 2;
    instNode->outputs[module_get_output_slot(mod, "out")] = data + 3;

    if (instNode->setup && !instNode->setup(instNode)) {
        fprintf(stderr, "Error: %s: could not setup instrument\n", n->name);
        goto exit_err;
    }
    n->data = instNode;
    n->isSetup = 1;
    return 1;

exit_err:
    if (mi) {
        module_free_import(mod);
    }
    free(mod);
    if (instNode) {
        free((char*)instNode->name);
    }
    free(instNode);
    free(data);
    free(fullInstPath);
    return 0;
}

static int keyboard_teardown(struct Node* n) {
    struct Node* inst;

    if (n->isSetup) {
        inst = n->data;

        free(inst->inputs[module_get_input_slot(inst->module, "frequency")]);
        module_free_import((struct Module*)inst->module);
        free((void*)inst->module);
        node_free(inst);
        free(inst);
    }
    return 1;
}

static int note_comp(const void* a, const void* b) {
    const struct Note *n1 = a, *n2 = b;

    if (n1->beat < n2->beat) return 1;
    if (n1->beat > n2->beat) return -1;
    if (n1->div < n2->div) return 1;
    if (n1->div > n2->div) return -1;
    return 0;
}

static int load_notes(struct Node* n,
                      struct Note** notes,
                      unsigned int* numNotes) {
    char *fullpath = NULL, *filename;
    unsigned int ok = 1;

    filename = n->inputs[SEQ]->content.str;

    if (!(fullpath = malloc(strlen(n->path) + strlen(filename) + 1))) {
        fprintf(stderr, "Error: load_notes: can't allocate fullpath\n");
    } else {
        strcpy(fullpath, n->path);
        strcpy(fullpath + strlen(n->path), filename);
        ok = sndk_load(fullpath, notes, numNotes);
    }
    free(fullpath);
    return ok;
}

static void inst_set_note(struct Node* inst, struct Note* note, float bpm) {
    struct Data *frequency, *velocity, *sustain;
    const struct Module* mod;
    float dt = 60. / bpm;

    mod = inst->module;
    frequency = inst->inputs[module_get_input_slot(mod, "frequency")];
    velocity = inst->inputs[module_get_input_slot(mod, "velocity")];
    sustain = inst->inputs[module_get_input_slot(mod, "sustain")];

    frequency->type = DATA_FLOAT;
    frequency->content.f = note->freq;
    velocity->type = DATA_FLOAT;
    velocity->content.f = note->veloc;
    sustain->type = DATA_FLOAT;
    sustain->content.f = note->sustain * dt;
}

static int buffer_mix(struct Buffer* dest,
                      struct Buffer* src,
                      unsigned int offset) {
    if (dest->size < offset + src->size) {
        void* tmp;
        unsigned int newSize = offset + src->size, i;

        if (!(tmp = realloc(dest->data, newSize * sizeof(float)))) {
            fprintf(stderr, "Error: buffer_mix: can't realloc buffer\n");
            return 0;
        }
        dest->data = tmp;
        for (i = dest->size; i < offset + src->size; i++) {
            dest->data[i] = 0.;
        }
        dest->size = newSize;
    }
    addbuf(dest->data + offset, src->data, src->size);
    return 1;
}

static int keyboard_process(struct Node* n) {
    struct Node* inst;
    const struct Module* mod;
    struct Buffer *outbuf, *instout;
    struct Note* notes;
    unsigned int numNotes, divs;
    float bpm, dt, divdt;

    GENERIC_CHECK_INPUTS(n, keyboard);

    inst = n->data;
    mod = inst->module;

    instout = &inst->outputs[module_get_output_slot(mod, "out")]->content.buf;

    n->outputs[0]->type = DATA_BUFFER;
    outbuf = &n->outputs[0]->content.buf;
    outbuf->samplingRate = 44100;
    outbuf->interp = INTERP_LINEAR;
    outbuf->size = 0;

    bpm = data_float(n->inputs[BPM], 0, 120);
    divs = data_float(n->inputs[DIV], 0, 4);

    dt = 60. / bpm;
    divdt = dt / divs;

    if (!load_notes(n, &notes, &numNotes)) {
        fprintf(stderr, "Error: %s: can't load notes\n", n->name);
    } else {
        unsigned int i, ok = 1;

        /* we process the later notes first to minimize the number of realloc
         * of the final array. It's more likely to get the max size from the
         * start this way.
         */
        qsort(notes, numNotes, sizeof(struct Note), note_comp);

        for (i = 0; ok && i < numNotes; i++) {
            unsigned int pos;

            pos = (notes[i].beat * dt + notes[i].div * divdt)
                  * outbuf->samplingRate;
            inst_set_note(inst, notes + i, bpm);
            if (!inst->process(inst)) {
                fprintf(stderr, "Error: %s: instrument failed\n", n->name);
                ok = 0;
            } else if (instout->samplingRate != outbuf->samplingRate) {
                fprintf(stderr, "Error: %s: instrument has bad sampling rate. "
                                "Needs 44100\n", n->name);
                ok = 0;
            } else if (!buffer_mix(outbuf, instout, pos)) {
                fprintf(stderr, "Error: %s: mixing failed\n", n->name);
                ok = 0;
            } else {
                node_flush_output(inst);
            }
        }
        free(notes);
        return ok;
    }
    return 0;
}
