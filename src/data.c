#include <stdlib.h>

#include "sndc.h"

void data_init(struct Data* data) {
    data->type = DATA_FLOAT;
    data->content.f = 0;
    data->ready = 0;
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
