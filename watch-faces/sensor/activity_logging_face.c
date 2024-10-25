/*
 * MIT License
 *
 * Copyright (c) 2022 Joey Castillo
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

#include <stdlib.h>
#include <string.h>
#include "activity_logging_face.h"
#include "filesystem.h"
#include "watch.h"

#ifdef HAS_ACCELEROMETER

// hacky: we're just tapping into Movement's global state.
// we should make better API for this.
extern uint32_t orientation_changes;
extern uint8_t active_minutes;

static void _activity_logging_face_log_data(activity_logging_state_t *state) {
    watch_date_time_t date_time = movement_get_local_date_time();
    size_t pos = state->data_points % ACTIVITY_LOGGING_NUM_DATA_POINTS;
    activity_logging_data_point_t data_point = {0};    

    data_point.bit.day = date_time.unit.day;
    data_point.bit.month = date_time.unit.month;
    data_point.bit.hour = date_time.unit.hour;
    data_point.bit.minute = date_time.unit.minute;
    data_point.bit.active_minutes = active_minutes;
    data_point.bit.orientation_changes = orientation_changes;
    // print size of thing
    printf("Size of data point: %d\n", sizeof(activity_logging_data_point_t));
    if (filesystem_append_file("activity.dat", (char *)&data_point, sizeof(activity_logging_data_point_t))) {
        printf("Data point written\n", state->data_points);
    } else {
        printf("Failed to write data point\n");
    }

    state->data[pos].reg = data_point.reg;
    state->data_points++;

    active_minutes = 0;
    orientation_changes = 0;
}

static void _activity_logging_face_update_display(activity_logging_state_t *state, bool clock_mode_24h) {
    int8_t pos = (state->data_points - 1 - state->display_index) % ACTIVITY_LOGGING_NUM_DATA_POINTS;
    char buf[8];

    watch_clear_indicator(WATCH_INDICATOR_24H);
    watch_clear_indicator(WATCH_INDICATOR_PM);
    watch_clear_colon();

    if (pos < 0) {
        // no data at this index
        watch_display_text_with_fallback(WATCH_POSITION_TOP, "ACT L", "AC");
        watch_display_text(WATCH_POSITION_BOTTOM, "no dat");
        sprintf(buf, "%2d", state->display_index);
        watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
    } else if (state->ts_ticks) {
        // we are displaying the timestamp in response to a button press
        watch_set_colon();
        if (clock_mode_24h) {
            watch_set_indicator(WATCH_INDICATOR_24H);
        } else {
            if (state->data[pos].bit.hour > 11) watch_set_indicator(WATCH_INDICATOR_PM);
            state->data[pos].bit.hour %= 12;
            if (state->data[pos].bit.hour == 0) state->data[pos].bit.hour = 12;
        }
        watch_display_text(WATCH_POSITION_TOP_LEFT, "AT");
        sprintf(buf, "%2d", state->data[pos].bit.day);
        watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
        sprintf(buf, "%2d%02d%02d", state->data[pos].bit.hour, state->data[pos].bit.minute, 0);
        watch_display_text(WATCH_POSITION_BOTTOM, buf);
    } else {
        // we are displaying the number of accelerometer wakeups and orientation changes
        watch_display_text(WATCH_POSITION_TOP, "AC");
        sprintf(buf, "%2d", state->display_index);
        watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
        sprintf(buf, "%-3lu/%2d", state->data[pos].bit.orientation_changes > 999 ? 999 : state->data[pos].bit.orientation_changes, state->data[pos].bit.active_minutes);
        watch_display_text(WATCH_POSITION_BOTTOM, buf);
    }
}

void activity_logging_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(activity_logging_state_t));
        memset(*context_ptr, 0, sizeof(activity_logging_state_t));
        // create file if it doesn't exist
        if (!filesystem_file_exists("activity.dat")) filesystem_write_file("activity.dat", "", 0);
    }
}

void activity_logging_face_activate(void *context) {
    activity_logging_state_t *state = (activity_logging_state_t *)context;
    state->display_index = 0;
    state->ts_ticks = 0;
}

bool activity_logging_face_loop(movement_event_t event, void *context) {
    activity_logging_state_t *state = (activity_logging_state_t *)context;
    switch (event.event_type) {
        case EVENT_TIMEOUT:
            movement_move_to_face(0);
            break;
        case EVENT_LIGHT_LONG_PRESS:
            // light button shows the timestamp, but if you need the light, long press it.
            movement_illuminate_led();
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            state->ts_ticks = 2;
            _activity_logging_face_update_display(state, movement_clock_mode_24h());
            break;
        case EVENT_ALARM_BUTTON_DOWN:
            state->display_index = (state->display_index + 1) % ACTIVITY_LOGGING_NUM_DATA_POINTS;
            state->ts_ticks = 0;
            // fall through
        case EVENT_ACTIVATE:
            _activity_logging_face_update_display(state, movement_clock_mode_24h());
            break;
        case EVENT_TICK:
            if (state->ts_ticks && --state->ts_ticks == 0) {
                _activity_logging_face_update_display(state, movement_clock_mode_24h());
            }
            break;
        case EVENT_BACKGROUND_TASK:
            _activity_logging_face_log_data(state);
            break;
        default:
            movement_default_loop_handler(event);
            break;
    }

    return true;
}

void activity_logging_face_resign(void *context) {
    (void) context;
}

movement_watch_face_advisory_t activity_logging_face_advise(void *context) {
    (void) context;
    movement_watch_face_advisory_t retval = { 0 };

    // log data every 5 minutes
    retval.wants_background_task = (movement_get_local_date_time().unit.minute % 5) == 0;

    return retval;
}

#endif
