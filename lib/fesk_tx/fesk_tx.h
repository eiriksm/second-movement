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

#define FESK_TICKS_PER_BIT 1
#define FESK_TICKS_PER_REST 1
#define FESK_BITS_PER_CODE 6
#define FESK_START_MARKER 62u
#define FESK_END_MARKER 63u

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
