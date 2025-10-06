#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FESK_OK = 0,
    FESK_ERR_INVALID_ARGUMENT,
    FESK_ERR_UNSUPPORTED_CHARACTER,
    FESK_ERR_ALLOCATION_FAILED,
} fesk_result_t;

fesk_result_t fesk_encode_text(const char *text,
                               size_t length,
                               int8_t **out_sequence,
                               size_t *out_entries);

fesk_result_t fesk_encode_cstr(const char *text,
                               int8_t **out_sequence,
                               size_t *out_entries);

void fesk_free_sequence(int8_t *sequence);

#ifdef __cplusplus
}
#endif

