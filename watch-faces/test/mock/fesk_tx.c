#include "fesk_tx.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

fesk_result_t fesk_encode(const char *text,
                          int8_t **out_sequence,
                          size_t *out_entries) {
    if (!text || !out_sequence) {
        return FESK_ERR_INVALID_ARGUMENT;
    }

    int8_t *sequence = (int8_t *)malloc(sizeof(int8_t));
    if (!sequence) {
        return FESK_ERR_ALLOCATION_FAILED;
    }

    sequence[0] = 0;
    *out_sequence = sequence;
    if (out_entries) {
        *out_entries = 0;
    }
    return FESK_OK;
}

fesk_result_t fesk_encode_text(const char *text,
                               size_t length,
                               int8_t **out_sequence,
                               size_t *out_entries) {
    if (!text || length == 0 || !out_sequence) {
        return FESK_ERR_INVALID_ARGUMENT;
    }

    return fesk_encode(text, out_sequence, out_entries);
}

fesk_result_t fesk_encode_cstr(const char *text,
                               int8_t **out_sequence,
                               size_t *out_entries) {
    if (!text) {
        return FESK_ERR_INVALID_ARGUMENT;
    }
    return fesk_encode_text(text, strlen(text), out_sequence, out_entries);
}

void fesk_free_sequence(int8_t *sequence) {
    free(sequence);
}
