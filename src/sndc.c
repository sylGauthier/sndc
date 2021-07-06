#include <string.h>
#include <stdlib.h>

#include "sndc.h"
#include "parser.h"

char sndcPath[MAX_SNDC_PATH][MAX_PATH_LENGTH];

static void list_modules() {
    unsigned int i;
    printf("Available modules:\n");
    for (i = 0; modules[i]; i++) {
        printf("    %s - %s\n", modules[i]->name, modules[i]->desc);
    }
}

static void print_type(int type) {
    char* sep = "";

    if (!type) {
        printf("UNKOWN");
        return;
    }
    if (type & DATA_BUFFER) {
        printf("%sBUFFER", sep);
        sep = " | ";
    }
    if (type & DATA_FLOAT) {
        printf("%sFLOAT", sep);
        sep = " | ";
    }
    if (type & DATA_STRING) {
        printf("%sSTRING", sep);
        sep = " | ";
    }
    if (type & DATA_NODE) {
        printf("%sNODE", sep);
        sep = " | ";
    }
}

static void print_module(const struct Module* module) {
    unsigned int i;
    printf("%s - %s\n", module->name, module->desc);
    printf("Inputs:\n");
    for (i = 0; i < MAX_INPUTS; i++) {
        if (module->inputs[i].name) {
            printf("    %s [", module->inputs[i].name);
            print_type(module->inputs[i].type);
            printf("] [%s]\n", module->inputs[i].req ?
                               "REQUIRED" : "OPTIONAL");
        }
    }
    printf("Outputs:\n");
    for (i = 0; i < MAX_OUTPUTS; i++) {
        if (module->outputs[i].name) {
            printf("    %s [", module->outputs[i].name);
            print_type(module->outputs[i].type);
            printf("]\n");
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
    if (argc <= 2) {
        printf("Usage: %s [-l]\n"
               "       %s [-h [module]]\n"
               "       %s inFile [outFile]\n",
               argv[0], argv[0], argv[0]);
        printf("Options:\n");
        printf("    -l: list available modules\n");
        printf("    -h: print this help\n");
        printf("    -h <module>: print module specification\n");
        printf("If no output file specified, will write to stdout.\n");
        return 0;
    } else if (argc > 2) {
        return help_module(argv[2]);
    }
    return 1;
}

static int init_path() {
    const char* envpath;
    unsigned int i = 0, j = 0;

    if ((envpath = getenv("SNDCPATH"))) {
        const char* cur = envpath;

        while (*cur) {
            if (i >= MAX_SNDC_PATH) {
                fprintf(stderr, "Warning: SNDCPATH has too many paths\n");
                break;
            } else if (*cur == ':') {
                i++;
                j = 0;
                cur++;
            } else if (j < MAX_PATH_LENGTH - 1) {
                sndcPath[i][j++] = *cur;
                cur++;
            } else {
                fprintf(stderr, "Warning: SNDCPATH too long\n");
                sndcPath[i][0] = '\0';
                cur = strchr(cur, ':');
            }
        }
    }
    return 1;
}

int main(int argc, char** argv) {
    struct Stack s;
    struct SNDCFile file = {0};
    FILE *out = NULL;
    char ok = 0, sndcInit = 0, stackInit = 0;

    if (argc < 2) {
        help(argc, argv);
        return 1;
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

    init_path();
    stack_init(&s);
    if (!(sndcInit = parse_sndc(&file, argv[1]))) {
        fprintf(stderr, "Error: parsing failed\n");
    } else if (!(stackInit = stack_load(&s, &file))) {
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
    if (stackInit) stack_free(&s);
    if (sndcInit) free_sndc(&file);
    if (out) fclose(out);

    return !ok;
}
