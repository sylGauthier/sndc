#include <stdlib.h>
#include <string.h>

#include "sndc.h"

char sndcPath[MAX_SNDC_PATH][MAX_PATH_LENGTH];

int path_init() {
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
