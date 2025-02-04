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

#ifndef SECRET_FACE_H_
#define SECRET_FACE_H_

#include "movement.h"

typedef struct {
    uint32_t target_ts;
    uint32_t now_ts;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t set_hours;
    uint8_t set_minutes;
    uint8_t set_seconds;
    uint8_t selection;
    uint8_t watch_face_index;
    uint8_t offset;
    uint8_t stopOffset;
} secret_state_t;


void secret_face_setup(uint8_t watch_face_index, void ** context_ptr);
void secret_face_activate(void *context);
bool secret_face_loop(movement_event_t event, void *context);
void secret_face_resign(void *context);

#define secret_face ((const watch_face_t){ \
    atb_countdown_face_setup, \
    atb_countdown_face_activate, \
    atb_countdown_face_loop, \
    atb_countdown_face_resign, \
    NULL, \
})

#endif // SECRET_FACE_H_
