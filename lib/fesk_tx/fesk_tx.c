#include "fesk_tx.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "watch_tcc.h"

#define FESK_TICKS_PER_SYMBOL 4
#define FESK_TICKS_PER_REST 4
#define FESK_PREAMBLE_TONE_TICKS 20
#define FESK_FRAME_SYMBOL_DIGITS 3
#define FESK_CRC_DIGITS 3
#define FESK_CRC_MASK 0x1F
#define FESK_TONE_LOG_CAP 1024
#define FESK_SYMBOL_LOG_CAP 1024

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

static void _append_to_log(char *buffer,
                           size_t *offset,
                           size_t capacity,
                           const char *fmt, ...) {
    if (!buffer || !offset || *offset >= capacity) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buffer + *offset, capacity - *offset, fmt, args);
    va_end(args);

    if (written < 0) {
        buffer[capacity - 1] = '\0';
        *offset = capacity;
        return;
    }

    size_t added = (size_t)written;
    if (added >= capacity - *offset) {
        *offset = capacity - 1;
        buffer[capacity - 1] = '\0';
    } else {
        *offset += added;
    }
}

static void _append_digit_to_log(char *buffer,
                                 size_t *offset,
                                 size_t capacity,
                                 uint8_t digit) {
    _append_to_log(buffer,
                   offset,
                   capacity,
                   *offset == 0 ? "%u" : " %u",
                   digit);
}

static inline void _append_digit(uint8_t digit,
                                 int8_t *sequence,
                                 size_t *pos,
                                 char *tone_log,
                                 size_t *tone_offset,
                                 size_t tone_capacity) {
    watch_buzzer_note_t tone = _tone_map[digit];
    sequence[(*pos)++] = (int8_t)tone;
    sequence[(*pos)++] = FESK_TICKS_PER_SYMBOL;
    sequence[(*pos)++] = (int8_t)BUZZER_NOTE_REST;
    sequence[(*pos)++] = FESK_TICKS_PER_REST;

    if (tone_log) {
        _append_digit_to_log(tone_log, tone_offset, tone_capacity, digit);
    }
}

static void _append_symbol_entry(char *buffer,
                                 size_t *offset,
                                 size_t capacity,
                                 const char *label,
                                 const uint8_t *digits,
                                 size_t digit_count) {
    if (!buffer || !label || !digits) {
        return;
    }

    _append_to_log(buffer,
                   offset,
                   capacity,
                   *offset == 0 ? "%s:[" : " %s:[",
                   label);

    for (size_t i = 0; i < digit_count; ++i) {
        _append_to_log(buffer,
                       offset,
                       capacity,
                       i == 0 ? "%u" : " %u",
                       digits[i]);
    }

    _append_to_log(buffer, offset, capacity, "]");
}

static void _format_symbol_name(unsigned char raw,
                                char *out,
                                size_t capacity) {
    if (!out || capacity == 0) {
        return;
    }

    if (raw == ' ') {
        snprintf(out, capacity, "' '");
    } else if (raw >= 32 && raw <= 126 && raw != '\'' && raw != '\\') {
        snprintf(out, capacity, "'%c'", raw);
    } else {
        snprintf(out, capacity, "0x%02X", raw);
    }
}

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
                                         size_t *pos,
                                         char *tone_log,
                                         size_t *tone_offset,
                                         size_t tone_capacity) {
    for (uint8_t d = 0; d < symbol->count; d++) {
        uint8_t digit = symbol->digits[d];
        _append_digit(digit, sequence, pos, tone_log, tone_offset, tone_capacity);
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

    char tone_log[FESK_TONE_LOG_CAP];
    char symbol_log[FESK_SYMBOL_LOG_CAP];
    size_t tone_log_offset = 0;
    size_t symbol_log_offset = 0;
    tone_log[0] = '\0';
    symbol_log[0] = '\0';

    uint8_t preamble_digit = 0;
    _append_symbol_entry(symbol_log,
                         &symbol_log_offset,
                         FESK_SYMBOL_LOG_CAP,
                         "PRE",
                         &preamble_digit,
                         1);

    size_t pos = 0;
    _append_note(sequence, &pos, _tone_map[0], FESK_PREAMBLE_TONE_TICKS);
    _append_digit_to_log(tone_log, &tone_log_offset, FESK_TONE_LOG_CAP, preamble_digit);

    _append_symbol_entry(symbol_log,
                         &symbol_log_offset,
                         FESK_SYMBOL_LOG_CAP,
                         "FRAME",
                         _symbol_frame_marker.digits,
                         _symbol_frame_marker.count);
    _append_symbol_digits(&_symbol_frame_marker,
                          sequence,
                          &pos,
                          tone_log,
                          &tone_log_offset,
                          FESK_TONE_LOG_CAP);

    for (size_t i = 0; i < length; i++) {
        unsigned char raw = (unsigned char)text[i];
        const fesk_symbol_t *symbol = _lookup_symbol(raw);
        char name[16];
        _format_symbol_name(raw, name, sizeof(name));
        _append_symbol_entry(symbol_log,
                             &symbol_log_offset,
                             FESK_SYMBOL_LOG_CAP,
                             name,
                             symbol->digits,
                             symbol->count);
        _append_symbol_digits(symbol,
                              sequence,
                              &pos,
                              tone_log,
                              &tone_log_offset,
                              FESK_TONE_LOG_CAP);
    }

    char crc_label[16];
    snprintf(crc_label, sizeof(crc_label), "CRC(0x%02X)", crc);
    _append_symbol_entry(symbol_log,
                         &symbol_log_offset,
                         FESK_SYMBOL_LOG_CAP,
                         crc_label,
                         crc_digits,
                         FESK_CRC_DIGITS);

    for (uint8_t d = 0; d < FESK_CRC_DIGITS; d++) {
        uint8_t digit = crc_digits[d];
        _append_digit(digit,
                      sequence,
                      &pos,
                      tone_log,
                      &tone_log_offset,
                      FESK_TONE_LOG_CAP);
    }

    _append_symbol_entry(symbol_log,
                         &symbol_log_offset,
                         FESK_SYMBOL_LOG_CAP,
                         "FRAME",
                         _symbol_frame_marker.digits,
                         _symbol_frame_marker.count);
    _append_symbol_digits(&_symbol_frame_marker,
                          sequence,
                          &pos,
                          tone_log,
                          &tone_log_offset,
                          FESK_TONE_LOG_CAP);

    sequence[pos] = 0;

    if (tone_log[0] != '\0') {
        printf("FESK tones: %s\n", tone_log);
    }
    if (symbol_log[0] != '\0') {
        printf("FESK symbols: %s\n", symbol_log);
    }

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
