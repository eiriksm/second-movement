#include "fesk_tx.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

#include "watch_tcc.h"

#define FESK_TICKS_PER_SYMBOL 4
#define FESK_PREAMBLE_TONE_TICKS 20
#define FESK_FRAME_SYMBOL_DIGITS 3
#define FESK_CRC_DIGITS 3
#define FESK_CRC_MASK 0x1F

typedef struct {
    uint8_t digits[4];
    uint8_t count;
} fesk_symbol_t;

static const fesk_symbol_t _letter_table[26] = {
    {{0, 0, 0, 0}, 2}, // a -> 00
    {{0, 1, 0, 0}, 2}, // b -> 01
    {{0, 2, 0, 0}, 2}, // c -> 02
    {{0, 3, 0, 0}, 2}, // d -> 03
    {{1, 0, 0, 0}, 2}, // e -> 10
    {{1, 1, 0, 0}, 2}, // f -> 11
    {{1, 2, 0, 0}, 2}, // g -> 12
    {{1, 3, 0, 0}, 2}, // h -> 13
    {{2, 0, 0, 0}, 2}, // i -> 20
    {{2, 1, 0, 0}, 2}, // j -> 21
    {{2, 2, 0, 0}, 2}, // k -> 22
    {{2, 3, 0, 0}, 2}, // l -> 23
    {{3, 0, 0, 0}, 3}, // m -> 300
    {{3, 0, 1, 0}, 3}, // n -> 301
    {{3, 0, 2, 0}, 3}, // o -> 302
    {{3, 0, 3, 0}, 3}, // p -> 303
    {{3, 1, 0, 0}, 4}, // q -> 3100
    {{3, 1, 1, 0}, 3}, // r -> 311
    {{3, 1, 2, 0}, 3}, // s -> 312
    {{3, 1, 3, 0}, 3}, // t -> 313
    {{3, 2, 0, 0}, 4}, // u -> 3200
    {{3, 2, 1, 0}, 3}, // v -> 321
    {{3, 2, 2, 0}, 3}, // w -> 322
    {{3, 2, 3, 0}, 3}, // x -> 323
    {{3, 3, 0, 0}, 4}, // y -> 3300
    {{3, 3, 1, 0}, 4}, // z -> 3310
};

static const fesk_symbol_t _digit_table[10] = {
    {{3, 3, 2, 0}, 3}, // 0 -> 332
    {{3, 3, 0, 1}, 4}, // 1 -> 3301
    {{3, 3, 0, 2}, 4}, // 2 -> 3302
    {{3, 3, 0, 3}, 4}, // 3 -> 3303
    {{3, 2, 0, 1}, 4}, // 4 -> 3201
    {{3, 2, 0, 2}, 4}, // 5 -> 3202
    {{3, 2, 0, 3}, 4}, // 6 -> 3203
    {{3, 1, 0, 1}, 4}, // 7 -> 3101
    {{3, 1, 0, 2}, 4}, // 8 -> 3102
    {{3, 1, 0, 3}, 4}, // 9 -> 3103
};

static const fesk_symbol_t _symbol_space = {{3, 3, 1, 1}, 4}; // space -> 3311
static const fesk_symbol_t _symbol_comma = {{3, 3, 1, 2}, 4}; // comma -> 3312
static const fesk_symbol_t _symbol_colon = {{3, 3, 1, 3}, 4}; // colon -> 3313
static const fesk_symbol_t _symbol_frame_marker = {{3, 3, 3, 0}, FESK_FRAME_SYMBOL_DIGITS}; // frame marker -> 333

static const watch_buzzer_note_t _tone_map[4] = {
    [0] = BUZZER_NOTE_F7,
    [1] = BUZZER_NOTE_A7,
    [2] = BUZZER_NOTE_D8,
    [3] = BUZZER_NOTE_G6,
};

static inline void _append_note(int8_t *sequence,
                                size_t *pos,
                                watch_buzzer_note_t tone,
                                int8_t tone_ticks) {
    sequence[(*pos)++] = (int8_t)tone;
    sequence[(*pos)++] = tone_ticks;
    sequence[(*pos)++] = (int8_t)BUZZER_NOTE_REST;
    sequence[(*pos)++] = FESK_TICKS_PER_SYMBOL;
}

static inline void _append_symbol_digits(const fesk_symbol_t *symbol,
                                         int8_t *sequence,
                                         size_t *pos) {
    for (uint8_t d = 0; d < symbol->count; d++) {
        uint8_t digit = symbol->digits[d];
        _append_note(sequence, pos, _tone_map[digit], FESK_TICKS_PER_SYMBOL);
    }
}

static const fesk_symbol_t *_lookup_symbol(unsigned char raw) {
    if (raw >= '0' && raw <= '9') {
        return &_digit_table[raw - '0'];
    }

    int lower = tolower(raw);
    if (lower >= 'a' && lower <= 'z') {
        return &_letter_table[lower - 'a'];
    }

    switch (raw) {
        case ' ': return &_symbol_space;
        case ',': return &_symbol_comma;
        case ':': return &_symbol_colon;
        default:  return NULL;
    }
}

static fesk_result_t _encode_internal(const char *text,
                                      size_t length,
                                      int8_t **out_sequence,
                                      size_t *out_entries) {
    if (!text || !out_sequence || length == 0) {
        return FESK_ERR_INVALID_ARGUMENT;
    }

    size_t total_digits = 0;
    uint8_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char raw = (unsigned char)text[i];
        const fesk_symbol_t *symbol = _lookup_symbol(raw);
        if (symbol == NULL) {
            return FESK_ERR_UNSUPPORTED_CHARACTER;
        }
        total_digits += symbol->count;
        for (uint8_t d = 0; d < symbol->count; d++) {
            uint8_t digit = symbol->digits[d];
            crc = (uint8_t)((crc + digit) & FESK_CRC_MASK);
        }
    }

    uint8_t crc_digits[FESK_CRC_DIGITS] = {
        (uint8_t)(crc & 0x03),
        (uint8_t)((crc >> 2) & 0x03),
        (uint8_t)((crc >> 4) & 0x03),
    };

    size_t framing_digits = (2 * FESK_FRAME_SYMBOL_DIGITS) + FESK_CRC_DIGITS;
    size_t total_digits_with_framing = total_digits + framing_digits;
    size_t total_entries = (total_digits_with_framing * 4) + 4;
    int8_t *sequence = malloc((total_entries + 1) * sizeof(int8_t));
    if (!sequence) {
        return FESK_ERR_ALLOCATION_FAILED;
    }

    size_t pos = 0;
    _append_note(sequence, &pos, _tone_map[0], FESK_PREAMBLE_TONE_TICKS);
    _append_symbol_digits(&_symbol_frame_marker, sequence, &pos);

    for (size_t i = 0; i < length; i++) {
        unsigned char raw = (unsigned char)text[i];
        const fesk_symbol_t *symbol = _lookup_symbol(raw);
        _append_symbol_digits(symbol, sequence, &pos);
    }

    for (uint8_t d = 0; d < FESK_CRC_DIGITS; d++) {
        uint8_t digit = crc_digits[d];
        _append_note(sequence, &pos, _tone_map[digit], FESK_TICKS_PER_SYMBOL);
    }

    _append_symbol_digits(&_symbol_frame_marker, sequence, &pos);

    sequence[pos] = 0;

    if (out_entries) {
        *out_entries = pos;
    }
    *out_sequence = sequence;

    return FESK_OK;
}

fesk_result_t fesk_encode_text(const char *text,
                               size_t length,
                               int8_t **out_sequence,
                               size_t *out_entries) {
    return _encode_internal(text, length, out_sequence, out_entries);
}

fesk_result_t fesk_encode_cstr(const char *text,
                               int8_t **out_sequence,
                               size_t *out_entries) {
    if (!text) {
        return FESK_ERR_INVALID_ARGUMENT;
    }

    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }

    return _encode_internal(text, length, out_sequence, out_entries);
}

void fesk_free_sequence(int8_t *sequence) {
    if (sequence) {
        free(sequence);
    }
}
