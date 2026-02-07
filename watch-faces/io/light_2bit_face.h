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
 * 2-BIT LIGHT SENSING PROTOCOL
 *
 * Receives data via 4 discrete light levels sensed through the IR photodiode,
 * encoding 2 bits per symbol using Gray code for noise immunity.
 *
 * Protocol overview:
 *   - 4 light levels map to 2-bit Gray-coded symbols: L0=00, L1=01, L2=11, L3=10
 *   - Auto-calibration phase measures ambient min/max to set adaptive thresholds
 *   - Frame: [SYNC x8] [START x4] [LEN x4] [DATA x4*N] [CRC8 x4]
 *     SYNC  = alternating 00,11 symbols (calibrates receiver)
 *     START = 01,10,01,10 (unique marker)
 *     LEN   = 1 byte (max 255 payload bytes), MSB-first, 4 symbols
 *     DATA  = N payload bytes, MSB-first, 4 symbols each
 *     CRC8  = XOR checksum of LEN+DATA bytes, 4 symbols
 *
 * Symbol rates (configurable via ALARM button):
 *   - 64 Hz  (128 bps, 16 B/s)
 *   - 128 Hz (256 bps, 32 B/s) -- default
 *
 * Display:
 *   - Top: "Li 2b" (Light 2-bit) or mode indicator
 *   - Bottom: status/data depending on state
 *
 * Buttons:
 *   - ALARM: cycle symbol rate / start-stop
 *   - LIGHT: suppressed (interferes with photodiode)
 *   - MODE: next face
 */

#define LIGHT_2BIT_MAX_PAYLOAD 128
#define LIGHT_2BIT_SYNC_COUNT  8
#define LIGHT_2BIT_START_LEN   4

typedef enum {
    LIGHT_2BIT_STATE_IDLE,       // waiting, showing status
    LIGHT_2BIT_STATE_CALIBRATE,  // measuring min/max for threshold calc
    LIGHT_2BIT_STATE_SYNC,       // looking for sync pattern
    LIGHT_2BIT_STATE_START,      // looking for start marker
    LIGHT_2BIT_STATE_LENGTH,     // reading length byte
    LIGHT_2BIT_STATE_DATA,       // reading payload bytes
    LIGHT_2BIT_STATE_CRC,        // reading CRC byte
    LIGHT_2BIT_STATE_DONE,       // frame received, showing result
    LIGHT_2BIT_STATE_ERROR,      // error, showing what went wrong
} light_2bit_state_t;

typedef struct {
    light_2bit_state_t state;

    // calibration
    uint16_t cal_min;
    uint16_t cal_max;
    uint16_t cal_samples;
    uint16_t thresholds[3]; // boundaries between levels 0-1, 1-2, 2-3

    // symbol rate: index into {64, 128}
    uint8_t rate_index;

    // receive state machine
    uint8_t sync_count;          // consecutive valid sync symbols seen
    uint8_t start_index;         // position in start marker matching
    uint8_t symbol_buf;          // accumulates symbols within a byte (4 symbols = 1 byte)
    uint8_t symbol_count;        // symbols accumulated in current byte
    uint8_t payload_len;         // expected payload length
    uint16_t payload_index;      // bytes received so far
    uint8_t crc_accum;           // running XOR checksum
    uint8_t last_symbol;         // last decoded symbol (for display/debug)

    // receive buffer
    uint8_t payload[LIGHT_2BIT_MAX_PAYLOAD];

    // burst mode: tight-loop ADC reads within one tick
    bool burst_mode;
    uint16_t burst_count;        // samples taken in burst
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
