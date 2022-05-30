#include <stdlib.h>
#include <string.h>

#include "sndc.h"

void data_init(struct Data* data) {
    memset(data, 0, sizeof(*data));
    data->type = DATA_FLOAT;
}

void data_free(struct Data* data) {
    if (data) {
        switch (data->type) {
            case DATA_BUFFER:
                free(data->content.buf.data);
                data->content.buf.data = NULL;
                data->content.buf.size = 0;
                return;
            case DATA_STRING:
                free(data->content.str);
                data->content.str = NULL;
                return;
            default:
                return;
        }
    }
}
