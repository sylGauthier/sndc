#include <stdlib.h>
#include <string.h>

#include "sndc.h"

void data_init(struct Data* data) {
    memset(data, 0, sizeof(*data));
    data->type = DATA_FLOAT;
}

void data_free(struct Data* data) {
    switch (data->type) {
        case DATA_BUFFER:
            free(data->content.buf.data);
            return;
        case DATA_STRING:
            free(data->content.str);
        default:
            return;
    }
}
