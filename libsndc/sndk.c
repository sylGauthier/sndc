#include <stdlib.h>

#include "sndc.h"
#include "modules/utils.h"

int sndk_load(const char* filename,
              struct Note** notes,
              unsigned int* numNotes) {
    FILE* kfile = NULL;
    unsigned int noteSize = 16, ok = 1;

    *notes = NULL;
    *numNotes = 0;

    if (!(kfile = fopen(filename, "r"))) {
        fprintf(stderr, "Error: sndk_load: can't open notes file: %s\n",
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
                if ((curNote->pitchID =
                            note_to_freq(note, &curNote->freq)) < 0) {
                    fprintf(stderr, "Error: %s: invalid note: %s\n",
                            filename, note);
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
    if (kfile) fclose(kfile);
    return ok;
}

int sndk_load_fixed(const char* filename,
                    struct Note* notes,
                    unsigned int* numNotes,
                    unsigned int size) {
    FILE* kfile = NULL;
    unsigned int ok = 1;

    *numNotes = 0;

    if (!(kfile = fopen(filename, "r"))) {
        fprintf(stderr, "Error: sndk_load: can't open notes file: %s\n",
                filename);
    } else {
        struct Note* curNote = notes;
        int n;
        char note[4];

        while ((n = fscanf(kfile, "%u:%u %3s %f %f\n",
                           &curNote->beat,
                           &curNote->div,
                           note,
                           &curNote->veloc,
                           &curNote->sustain)) != EOF) {
            if (*numNotes >= size) {
                fprintf(stderr, "Error: %s: too many notes\n",
                        filename);
                ok = 0;
                break;
            }
            if (n == 5) {
                (*numNotes)++;
                if ((curNote->pitchID =
                            note_to_freq(note, &curNote->freq)) < 0) {
                    fprintf(stderr, "Error: %s: invalid note: %s\n",
                            filename, note);
                    ok = 0;
                    break;
                }
                curNote = notes + *numNotes;
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
        *numNotes = 0;
    }
    if (kfile) fclose(kfile);
    return ok;
}
