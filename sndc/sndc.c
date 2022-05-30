#include <string.h>
#include <stdlib.h>

#include "sndc.h"

static int comp_module(const void* m1, const void* m2) {
    struct Module* const *mod1 = m1;
    struct Module* const *mod2 = m2;
    int r;

    r = strcmp((*mod1)->category, (*mod2)->category);
    if (r < 0) return -1; else if (r > 0) return 1;

    r = strcmp((*mod1)->name, (*mod2)->name);
    if (r < 0) return -1; else if (r > 0) return 1;
    return 0;
}

static void list_modules() {
    unsigned int i;
    const char* curCat = "";
    printf("Available modules:\n");
    for (i = 0; i < numModules; i++) {
        if (strcmp(modules[i]->category, curCat)) {
            curCat = modules[i]->category;
            printf("\n  - %s:\n", curCat);
        }
        printf("    %s - %s\n", modules[i]->name, modules[i]->desc);
    }
}

static int print_type(int type) {
    char* sep = "";
    int l = 2;

    putc('[', stdout);
    if (!type) {
        printf("UNKOWN");
        l += 6;
    }
    if (type & DATA_BUFFER) {
        printf("BUFFER");
        l += 6;
        sep = " | ";
    }
    if (type & DATA_FLOAT) {
        printf("%sFLOAT", sep);
        l += 5 + strlen(sep);
        sep = " | ";
    }
    if (type & DATA_STRING) {
        printf("%sSTRING", sep);
        l += 6 + strlen(sep);
        sep = " | ";
    }
    putc(']', stdout);
    return l;
}

static void print_module(const struct Module* module) {
    unsigned int i;
    printf("[%s] %s - %s\n", module->category, module->name, module->desc);
    printf("Inputs:\n");
    for (i = 0; i < MAX_INPUTS; i++) {
        if (module->inputs[i].name) {
            printf("    %s ", module->inputs[i].name);
            print_type(module->inputs[i].type);
            printf(" [%s]\n        %s\n",
                    module->inputs[i].req ? "REQUIRED" : "OPTIONAL",
                    module->inputs[i].description);
        }
    }
    printf("Outputs:\n");
    for (i = 0; i < MAX_OUTPUTS; i++) {
        if (module->outputs[i].name) {
            printf("    %s ", module->outputs[i].name);
            print_type(module->outputs[i].type);
            printf(" %s\n", module->outputs[i].description);
        }
    }
}

static int help_module(const char* module) {
    unsigned int i;

    for (i = 0; i < numModules; i++) {
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

int main(int argc, char** argv) {
    struct Stack s;
    struct SNDCFile file = {0};
    FILE *out = NULL;
    char ok = 0, sndcInit = 0, stackInit = 0;

    if (argc < 2) {
        help(argc, argv);
        return 1;
    } else if (!strcmp(argv[1], "-l")) {
        qsort(modules, numModules, sizeof(struct Module*), comp_module);
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

    path_init();
    stack_init(&s);
    s.verbose = 1;
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
