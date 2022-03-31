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
    "keyboard", "Melodic sequencer for a given intrument node",
    {
        {"instrument",  DATA_NODE,      REQUIRED,
                        "the instrument node, "
                        "a previously declared or imported node"},

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
    struct Node* inst;
    const struct Module* mod;
    struct Data* data;

    if (!n->inputs[INS] || n->inputs[INS]->type != DATA_NODE) {
        fprintf(stderr, "Error: %s:"
                        "instrument is required and must be of type NODE\n",
                        n->name);
        return 0;
    }
    inst = n->inputs[INS]->content.node;
    mod = inst->module;

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
        return 0;
    }

    if (!(data = calloc(3, sizeof(*data)))) {
        fprintf(stderr, "Error: %s: can't create data\n", n->name);
        return 0;
    }

    data[0].type = DATA_FLOAT;
    data[1].type = DATA_FLOAT;
    data[2].type = DATA_FLOAT;

    inst->inputs[module_get_input_slot(mod, "frequency")] = data;
    inst->inputs[module_get_input_slot(mod, "velocity")] = data + 1;
    inst->inputs[module_get_input_slot(mod, "sustain")] = data + 2;

    if (!inst->isSetup && inst->setup && !inst->setup(inst)) {
        fprintf(stderr, "Error: %s: could not setup instrument\n", n->name);
        free(data);
        return 0;
    }
    n->isSetup = 1;
    return 1;
}

static int keyboard_teardown(struct Node* n) {
    struct Node* inst;

    if (n->isSetup) {
        inst = n->inputs[INS]->content.node;

        free(inst->inputs[module_get_input_slot(inst->module, "frequency")]);
    }
    return 1;
}

struct Note {
    unsigned int beat;
    unsigned int div;

    float freq;
    float veloc;
    float sustain;
};

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
    FILE* kfile = NULL;
    char *fullpath = NULL, *filename;
    unsigned int noteSize = 16, ok = 1;

    filename = n->inputs[SEQ]->content.str;
    *notes = NULL;
    *numNotes = 0;

    if (!(fullpath = malloc(strlen(n->path) + strlen(filename) + 1))) {
        fprintf(stderr, "Error: load_notes: can't allocate fullpath\n");
    } else if (!strcpy(fullpath, n->path)
            || !strcpy(fullpath + strlen(n->path), filename)
            || !(kfile = fopen(fullpath, "r"))) {
        fprintf(stderr, "Error: load_notes: can't open notes file: %s\n",
                filename);
    } else if (!(*notes = calloc(noteSize, sizeof(struct Note)))) {
        fprintf(stderr, "Error: load_notes: can't allocate notes\n");
    } else {
        struct Note* curNote = *notes;
        int n;
        char note[4];

        while ((n = fscanf(kfile, "%u:%u %3s %f %f\n",
                           &curNote->beat,
                           &curNote->div,
                           note,
                           &curNote->veloc,
                           &curNote->sustain)) != EOF) {
            if (n == 5) {
                (*numNotes)++;
                if (!note_to_freq(note, &curNote->freq)) {
                    fprintf(stderr, "Error: %s: invalid note: %s\n",
                            fullpath, note);
                    ok = 0;
                    break;
                }
                if (*numNotes >= noteSize) {
                    void* tmp;

                    noteSize *= 2;
                    if (!(tmp = realloc(*notes, noteSize * sizeof(**notes)))) {
                        fprintf(stderr, "Error: load_notes: "
                                        "can't realloc notes\n");
                        ok = 0;
                        break;
                    }
                    *notes = tmp;
                }
                curNote = *notes + *numNotes;
            } else {
                char c;
                fprintf(stderr, "Warning: load_notes: "
                                "skipped malformatted line\n");
                do {
                      c = fgetc(kfile);
                } while (c != '\n');
            }
        }
    }
    if (!ok) {
        free(*notes);
        *notes = NULL;
        *numNotes = 0;
    }
    free(fullpath);
    if (kfile) fclose(kfile);
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

    inst = n->inputs[INS]->content.node;
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
