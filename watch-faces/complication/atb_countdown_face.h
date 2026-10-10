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

#ifndef ATB_COUNTDOWN_FACE_H_
#define ATB_COUNTDOWN_FACE_H_

/*
 * ATB COUNTDOWN
 *
 * Counts down to the next departures from a stop. LIGHT cycles through the
 * stops, ALARM through the next five departures from the selected stop.
 *
 * Stops are read from the file "atb.txt" if it exists, falling back to a few
 * built in stops otherwise. The format is described in the README of
 * https://github.com/eiriksm/atb.c, for example:
 *
 *   [UGL 9]
 *   offset  5
 *   mon-fri 05:57-18:12/15 18:42-23:42/30
 *   sat     07:12-08:42/30 09:12-18:12/15 18:42-23:42/30
 *   sun     09:12-23:42/30
 */

#include "movement.h"

#define ATB_COUNTDOWN_DEPARTURES 5

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
    // Number of stops, from the file or the built in list.
    uint8_t stop_count;
    bool use_file;
    // Departures and labels for stopOffset, valid while departures_valid.
    bool departures_valid;
    // Unix timestamps, 0 for no departure.
    uint32_t departures[ATB_COUNTDOWN_DEPARTURES];
    char label[4];
    char route_label[3];
    char fallback_label[3];
} atb_countdown_state_t;


void atb_countdown_face_setup(uint8_t watch_face_index, void ** context_ptr);
void atb_countdown_face_activate(void *context);
bool atb_countdown_face_loop(movement_event_t event, void *context);
void atb_countdown_face_resign(void *context);

#define atb_countdown_face ((const watch_face_t){ \
    atb_countdown_face_setup, \
    atb_countdown_face_activate, \
    atb_countdown_face_loop, \
    atb_countdown_face_resign, \
    NULL, \
})

#endif // ATB_COUNTDOWN_FACE_H_
