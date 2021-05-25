#include <string.h>

#include "sndc.h"

static void list_modules() {
    unsigned int i;
    printf("Available modules:\n");
    for (i = 0; modules[i]; i++) {
        printf("    %s - %s\n", modules[i]->name, modules[i]->desc);
    }
}

int main(int argc, char** argv) {
    struct Stack s;
    FILE *in = NULL, *out = NULL;
    char ok = 0;

    if (argc > 1 && !strcmp(argv[1], "-l")) {
        list_modules();
        return 0;
    }

    if (argc < 2) {
        in = stdin;
    } else {
        if (!(in = fopen(argv[1], "r"))) {
            fprintf(stderr, "Error: can't open %s for lecture\n", argv[1]);
            return 1;
        }
    }

    if (argc < 3) {
        out = stdout;
    } else {
        if (!(out = fopen(argv[2], "r"))) {
            fprintf(stderr, "Error: can't open %s for writing\n", argv[2]);
            fclose(in);
            return 1;
        }
    }

    stack_init(&s);
    if (!stack_load(&s, in)) {
        fprintf(stderr, "Error: loading stack failed\n");
    } else if (!stack_valid(&s)) {
        fprintf(stderr, "Error: stack invalid\n");
    } else if (!stack_process(&s)) {
        fprintf(stderr, "Error: processing stack failed\n");
    } else {
        if (s.numNodes) {
            struct Node* n = s.nodes[s.numNodes - 1];
            struct Data* data;

            if ((data = n->outputs[0]) && data->type == DATA_BUFFER) {
                fwrite(data->content.buf.data,
                        sizeof(float),
                        data->content.buf.size,
                        out);
            }
        }
        ok = 1;
    }
    stack_free(&s);
    if (in) fclose(in);
    if (out) fclose(out);

    return !ok;
}
