/*
 * MIT License
 *
 * Copyright (c) 2026 Eirik S. Morland
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

#ifndef DAY_COUNTDOWN_FACE_H_
#define DAY_COUNTDOWN_FACE_H_

/*
 * DAY COUNTDOWN
 *
 * Counts down the number of days until a set of events. The top shows the
 * label of the event and the main line the number of days left. Events that
 * have passed count up again, shown with a minus sign.
 *
 * ALARM cycles through the events. Holding ALARM shows the date of the event
 * as YYMMDD. LIGHT turns on the light as usual.
 *
 * Events are read from the file "days.txt", one per line, as a label and a
 * unix timestamp separated by a pipe. Only the first five characters of the
 * label are shown (two on the classic LCD). Empty lines and lines starting
 * with # are ignored. For example:
 *
 *   # Summer vacation, 1 July 2027
 *   VAC|1814400000
 *
 * Without the file, the face counts down to THAI on 1 February 2027.
 *
 * The events from the file (or THAI) are followed by XMAS, which always
 * counts down to the next Christmas Eve.
 */

#include "movement.h"

#define DAY_COUNTDOWN_MAX_FILE_COUNTERS 16

typedef struct {
    uint32_t ts;
    char label[6];
} day_countdown_counter_t;

typedef struct {
    day_countdown_counter_t counters[DAY_COUNTDOWN_MAX_FILE_COUNTERS];
    // Number of counters read from the file.
    uint8_t file_count;
    uint8_t selection;
    bool showing_date;
    // Local day the display was last drawn for, to redraw at midnight.
    int32_t drawn_day;
} day_countdown_state_t;

void day_countdown_face_setup(uint8_t watch_face_index, void ** context_ptr);
void day_countdown_face_activate(void *context);
bool day_countdown_face_loop(movement_event_t event, void *context);
void day_countdown_face_resign(void *context);

#define day_countdown_face ((const watch_face_t){ \
    day_countdown_face_setup, \
    day_countdown_face_activate, \
    day_countdown_face_loop, \
    day_countdown_face_resign, \
    NULL, \
})

#endif // DAY_COUNTDOWN_FACE_H_
