/*
 * MIT License
 *
 * Copyright (c) 2025 Eirik S. Morland
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "watch_tcc.h"

typedef enum {
    FESK_OK = 0,
    FESK_ERR_INVALID_ARGUMENT,
    FESK_ERR_UNSUPPORTED_CHARACTER,
    FESK_ERR_ALLOCATION_FAILED,
} fesk_result_t;

// The c32 PR changed watch_buzzer_play_sequence to subtract 1 from durations.
// Old code: duration N plays for N+1 callbacks
// New code: duration N plays for N callbacks
// To match timing, we add 1 to each duration when the new code is present.
#ifdef WATCH_BUZZER_PERIOD_REST
// New buzzer code: add 1 to compensate for the -1 in playback
#define FESK_TICKS_PER_BIT 2
#define FESK_TICKS_PER_REST 3
#else
// Old buzzer code: use original values
#define FESK_TICKS_PER_BIT 1
#define FESK_TICKS_PER_REST 2
#endif

#define FESK_BITS_PER_CODE 6
#define FESK_START_MARKER 62u
#define FESK_END_MARKER 63u

enum {
    FESK_TONE_ZERO = 0,
    FESK_TONE_ONE = 1,
    FESK_TONE_COUNT = 2,
};

#define FESK_TONE_LOW_NOTE  BUZZER_NOTE_D7SHARP_E7FLAT
#define FESK_TONE_HIGH_NOTE BUZZER_NOTE_G7

extern const watch_buzzer_note_t fesk_tone_map[FESK_TONE_COUNT];

fesk_result_t fesk_encode_text(const char *text,
                               size_t length,
                               int8_t **out_sequence,
                               size_t *out_entries);

fesk_result_t fesk_encode_cstr(const char *text,
                               int8_t **out_sequence,
                               size_t *out_entries);

void fesk_free_sequence(int8_t *sequence);

// Helper functions for raw source generation
bool fesk_lookup_char_code(unsigned char ch, uint8_t *out_code);
uint8_t fesk_crc8_update_code(uint8_t crc, uint8_t code);
