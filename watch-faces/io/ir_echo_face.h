/*
 * MIT License
 *
 * Copyright (c) 2026
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

/*
 * IR ECHO FACE
 *
 * Watch face that echoes back whatever is received via IR on the bottom
 * part of the screen.
 *
 * In hardware mode: Receives data via IR sensor and displays it
 * In simulator mode: Shows a blinking indicator to simulate activity
 *
 * Button functions:
 * - ALARM button: Clear the received data
 */

typedef struct {
    char received_data[7];  // 6 chars + null terminator for display
    bool has_data;
} ir_echo_state_t;

void ir_echo_face_setup(uint8_t watch_face_index, void ** context_ptr);
void ir_echo_face_activate(void *context);
bool ir_echo_face_loop(movement_event_t event, void *context);
void ir_echo_face_resign(void *context);

#define ir_echo_face ((const watch_face_t){ \
    ir_echo_face_setup, \
    ir_echo_face_activate, \
    ir_echo_face_loop, \
    ir_echo_face_resign, \
    NULL, \
})
