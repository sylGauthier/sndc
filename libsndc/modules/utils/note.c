
#include <sndc.h>
#include <modules/utils.h>

static int note_process(struct Node* n);

/* DECLARE_MODULE(note) */
const struct Module note = {
    "note", "util", "A simple note to frequency converter",
    {
        {"note",    DATA_STRING,    REQUIRED,
                    "the note's string representation, "
                    "must be of the format '[A-G][#b]?[0-9]' eg A#4, B3, etc"}
    },
    {
        {"freq",    DATA_FLOAT,     REQUIRED,
                    "the note's corresponding frequency"}
    },
    NULL,
    note_process,
    NULL
};

enum NoteInputType {
    NOTE,

    NUM_INPUTS
};

static int note_process(struct Node* n) {
    float f;

    GENERIC_CHECK_INPUTS(n, note);

    if (!(note_to_freq(n->inputs[NOTE]->content.str, &f))) {
        fprintf(stderr, "Error: %s: invalid note: %s\n",
                n->name, n->inputs[NOTE]->content.str);
        return 0;
    }

    n->outputs[0]->type = DATA_FLOAT;
    n->outputs[0]->content.f = f;

    return 1;
}
