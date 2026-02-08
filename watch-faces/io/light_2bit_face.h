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
 * BINARY LIGHT PROTOCOL FACE
 *
 * Receives arbitrary data (binary, UTF-8) via the IR photodiode using the
 * light binary protocol library (light_binary_protocol.h).
 *
 * Display:
 *   CAL    — calibrating (point light source at sensor, toggle on/off)
 *   SYNC   — waiting for sync preamble
 *   St     — matching start marker
 *   Ln     — reading length byte
 *   dNNN   — receiving data (NNN = bytes so far)
 *   CRC    — reading checksum
 *   RECV   — frame received OK (green LED)
 *   FAIL   — CRC or length error (red LED)
 *
 * Buttons:
 *   ALARM short: recalibrate (during sync), reset (after done/error),
 *                cycle rate (during idle)
 *   LIGHT:       suppressed (interferes with sensor)
 *   MODE:        next face
 */

#include "light_binary_protocol.h"

typedef struct {
    lbp_threshold_t threshold;
    lbp_decoder_t decoder;
    uint8_t rate_index;
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
