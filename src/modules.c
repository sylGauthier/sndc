#include <string.h>

#include "sndc.h"
#include "modules.h"

struct Module modules[MAX_MODULES];
unsigned int numModules;

static int load_module(int (*load)(struct Module*)) {
    if (numModules < MAX_MODULES) {
        memset(modules + numModules, 0, sizeof(struct Module));
        if (!load(modules + numModules)) return 0;
        numModules++;
        return 1;
    }
    return 0;
}

/* add new modules here */
int module_load_all() {
    if (!load_module(osc_load)) return 0;
    if (!load_module(mix_load)) return 0;

    return 1;
}

struct Module* module_find(const char* name) {
    unsigned int i;
    for (i = 0; i < numModules; i++) {
        if (!strcmp(modules[i].name, name)) return modules + i;
    }
    return NULL;
}

int module_get_input_slot(struct Module* module, const char* name) {
    unsigned int i;
    for (i = 0; i < MAX_INPUTS; i++) {
        if (module->inputNames[i] && !strcmp(module->inputNames[i], name)) {
            return i;
        }
    }
    return -1;
}

int module_get_output_slot(struct Module* module, const char* name) {
    unsigned int i;
    for (i = 0; i < MAX_OUTPUTS; i++) {
        if (module->outputNames[i] && !strcmp(module->outputNames[i], name)) {
            return i;
        }
    }
    return -1;
}
