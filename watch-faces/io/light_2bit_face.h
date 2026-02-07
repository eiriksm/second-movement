/*
 * MIT License
 *
 * Copyright (c) 2025 Claude
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

#include "movement.h"

#ifdef HAS_IR_SENSOR

/*
 * BINARY LIGHT SENSING PROTOCOL
 *
 * Receives data via 2 light levels (black/white) sensed through the IR
 * photodiode. One bit per symbol, single adaptive threshold with hysteresis.
 *
 * Protocol overview:
 *   - 2 levels: dark=0, bright=1. Single threshold at midpoint of calibrated range.
 *   - Hysteresis band around threshold prevents flickering near the boundary.
 *   - Auto-calibration measures min/max to set the threshold adaptively.
 *   - Frame format:
 *       [SYNC x16] [START x4] [LEN x8] [DATA x8*N] [CRC8 x8]
 *     SYNC  = alternating 0,1,0,1,... (16 symbols for clock recovery)
 *     START = 1,1,0,0 (breaks the alternating pattern as a unique marker)
 *     LEN   = 1 byte (max 128 payload bytes), MSB-first, 8 symbols
 *     DATA  = N payload bytes, MSB-first, 8 symbols each
 *     CRC8  = XOR checksum of LEN+DATA bytes, 8 symbols
 *
 * Symbol rates (configurable via ALARM button):
 *   - 64 Hz  (64 bps, 8 B/s)
 *   - 128 Hz (128 bps, 16 B/s) -- default
 *
 * Display:
 *   - Top: "LI bI" (Light Binary) or status
 *   - Bottom: state/data depending on phase
 *
 * Buttons:
 *   - ALARM short press: recalibrate (in sync), cycle rate (in idle), reset (after done/error)
 *   - LIGHT: suppressed (interferes with photodiode)
 *   - MODE: next face
 */

#define LIGHT_BIN_MAX_PAYLOAD  128
#define LIGHT_BIN_SYNC_COUNT   16
#define LIGHT_BIN_START_LEN    4

typedef enum {
    LIGHT_BIN_STATE_IDLE,
    LIGHT_BIN_STATE_CALIBRATE,
    LIGHT_BIN_STATE_SYNC,
    LIGHT_BIN_STATE_START,
    LIGHT_BIN_STATE_LENGTH,
    LIGHT_BIN_STATE_DATA,
    LIGHT_BIN_STATE_CRC,
    LIGHT_BIN_STATE_DONE,
    LIGHT_BIN_STATE_ERROR,
} light_bin_state_t;

typedef struct {
    light_bin_state_t state;

    // calibration
    uint16_t cal_min;
    uint16_t cal_max;
    uint16_t cal_samples;
    uint16_t threshold;     // midpoint between min and max
    uint16_t hysteresis;    // half-width of dead zone around threshold

    // symbol rate: index into {64, 128}
    uint8_t rate_index;

    // current decoded bit (with hysteresis applied)
    uint8_t current_bit;

    // receive state machine
    uint8_t sync_count;
    uint8_t start_index;
    uint8_t bit_buf;         // accumulates bits within a byte
    uint8_t bit_count;       // bits accumulated in current byte (0-7)
    uint8_t payload_len;
    uint16_t payload_index;
    uint8_t crc_accum;

    // receive buffer
    uint8_t payload[LIGHT_BIN_MAX_PAYLOAD];
} light_2bit_context_t;

void light_2bit_face_setup(uint8_t watch_face_index, void ** context_ptr);
void light_2bit_face_activate(void *context);
bool light_2bit_face_loop(movement_event_t event, void *context);
void light_2bit_face_resign(void *context);

#define light_2bit_face ((const watch_face_t){ \
    light_2bit_face_setup, \
    light_2bit_face_activate, \
    light_2bit_face_loop, \
    light_2bit_face_resign, \
    NULL, \
})

#endif // HAS_IR_SENSOR
