#include <string.h>

#include "sndc.h"
#include "parser.h"

static void list_modules() {
    unsigned int i;
    printf("Available modules:\n");
    for (i = 0; modules[i]; i++) {
        printf("    %s - %s\n", modules[i]->name, modules[i]->desc);
    }
}

static void print_module(const struct Module* module) {
    unsigned int i;
    printf("%s - %s\n", module->name, module->desc);
    printf("Inputs:\n");
    for (i = 0; i < MAX_INPUTS; i++) {
        if (module->inputs[i].name) {
            char* sep = "";
            printf("    %s [", module->inputs[i].name);
            if (!module->inputs[i].type) {
                printf("UNKOWN");
            } else {
                if (module->inputs[i].type & DATA_BUFFER) {
                    printf("%sBUFFER", sep);
                    sep = " | ";
                }
                if (module->inputs[i].type & DATA_FLOAT) {
                    printf("%sFLOAT", sep);
                    sep = " | ";
                }
                if (module->inputs[i].type & DATA_STRING) {
                    printf("%sSTRING", sep);
                    sep = " | ";
                }
            }
            printf("] [%s]\n", module->inputs[i].req ?
                               "REQUIRED" : "OPTIONAL");
        }
    }
}

static int help_module(const char* module) {
    unsigned int i;

    for (i = 0; modules[i]; i++) {
        if (!strcmp(modules[i]->name, module)) {
            print_module(modules[i]);
            return 0;
        }
    }
    fprintf(stderr, "Error: no such module: %s\n", module);
    return 1;
}

static int help(int argc, char** argv) {
    if (argc == 2) {
        printf("Usage: %s [-l]\n"
               "       %s [-h [module]]\n"
               "       %s [inFile [outFile]]\n",
               argv[0], argv[0], argv[0]);
        printf("Options:\n");
        printf("    -l: list available modules\n");
        printf("    -h: print this help\n");
        printf("    -h <module>: print module specification\n");
        printf("If no input file specified, will read from stdin.\n");
        printf("If no output file specified, will write to stdout.\n");
        return 0;
    } else if (argc > 2) {
        return help_module(argv[2]);
    }
    return 1;
}

int main(int argc, char** argv) {
    struct Stack s;
    struct SNDCFile file = {0};
    FILE *out = NULL;
    char ok = 0;

    if (argc < 2) {
        return help(argc, argv);
    } else if (!strcmp(argv[1], "-l")) {
        list_modules();
        return 0;
    } else if (!strcmp(argv[1], "-h")) {
        return help(argc, argv);
    }

    if (argc < 3) {
        out = stdout;
    } else {
        if (!(out = fopen(argv[2], "r"))) {
            fprintf(stderr, "Error: can't open %s for writing\n", argv[2]);
            return 1;
        }
    }

    stack_init(&s);
    if (!parse_sndc(&file, argv[1])) {
        fprintf(stderr, "Error: parsing failed\n");
    } else if (!stack_load(&s, &file)) {
        fprintf(stderr, "Error: loading stack failed\n");
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
    free_sndc(&file);
    if (out) fclose(out);

    return !ok;
}
