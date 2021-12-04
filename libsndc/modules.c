#include <string.h>

#include "sndc.h"
#include "modules.h"

const struct Module* module_find(const char* name) {
    unsigned int i;
    for (i = 0; modules[i]; i++) {
        if (!strcmp(modules[i]->name, name)) return modules[i];
    }
    return NULL;
}

int module_get_input_slot(const struct Module* module, const char* name) {
    unsigned int i;
    for (i = 0; i < MAX_INPUTS; i++) {
        if (module->inputs[i].name && !strcmp(module->inputs[i].name, name)) {
            return i;
        }
    }
    return -1;
}

int module_get_output_slot(const struct Module* module, const char* name) {
    unsigned int i;
    for (i = 0; i < MAX_OUTPUTS; i++) {
        if (module->outputs[i].name && !strcmp(module->outputs[i].name, name)) {
            return i;
        }
    }
    return -1;
}
