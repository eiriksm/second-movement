/*
 * MIT License
 *
 * Copyright (c) 2025 Eirik S. Morland
 * Copyright (c) 2024 Joseph Bryant
 * Copyright (c) 2023 Konrad Rieck
 * Copyright (c) 2022 Wesley Ellis
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
#include <time.h>
#include "atb_countdown_face.h"
#include "atb.h"
#include "filesystem.h"
#include "watch.h"
#include "watch_utility.h"
#include "zones.h"

static inline void store_countdown(atb_countdown_state_t *state) {
    /* Store set countdown time */
    state->set_hours = state->hours;
    state->set_minutes = state->minutes;
    state->set_seconds = state->seconds;
}

static inline void load_countdown(atb_countdown_state_t *state) {
    /* Load set countdown time */
    state->hours = state->set_hours;
    state->minutes = state->set_minutes;
    state->seconds = state->set_seconds;
}

#define ATB_FILE "atb.txt"

// Used when there is no schedule file.
static const struct {
    char *route;
    char *stop_id;
    AtbEntryInfo info;
    char fallback_label[3];
} builtin_stops[] = {
    {"09_2", "71779", {"UGL", " 9"}, "UA"},
    {"09_1", "74061", {"STO", " 9"}, "ST"},
    {"11_2", "71773", {"UGL", "11"}, "UA"},
    {"11_1", "74265", {"KON", "11"}, "KG"},
};

typedef struct {
    int32_t offset;
    int32_t size;
} atb_file_reader_t;

static bool read_atb_line(void *ctx, char *buf, int len) {
    atb_file_reader_t *reader = (atb_file_reader_t *)ctx;
    // filesystem_read_line does not report the end of the file by itself.
    if (reader->offset >= reader->size) {
        return false;
    }
    return filesystem_read_line(ATB_FILE, buf, &reader->offset, len - 1);
}

static atb_file_reader_t open_atb_file(void) {
    return (atb_file_reader_t){ .offset = 0, .size = filesystem_get_file_size(ATB_FILE) };
}

static void load_stops(atb_countdown_state_t *state) {
    state->use_file = false;
    state->stop_count = sizeof(builtin_stops) / sizeof(builtin_stops[0]);
    if (filesystem_file_exists(ATB_FILE)) {
        atb_file_reader_t reader = open_atb_file();
        int count = atb_file_count_entries(read_atb_line, &reader);
        if (count > 0) {
            state->use_file = true;
            state->stop_count = count > UINT8_MAX ? UINT8_MAX : count;
        }
    }
    if (state->stopOffset >= state->stop_count) {
        state->stopOffset = 0;
    }
    state->departures_valid = false;
}

static void load_departures(atb_countdown_state_t *state) {
    AtbEntryInfo info;
    ResultSet result_set;
    if (state->use_file) {
        atb_file_reader_t reader = open_atb_file();
        if (!atb_file_get_entry(read_atb_line, &reader, state->stopOffset, &info)) {
            memset(&info, 0, sizeof(info));
        }
        reader = open_atb_file();
        result_set = atb_file_next_departures(state->now_ts, read_atb_line, &reader, state->stopOffset);
        snprintf(state->fallback_label, sizeof(state->fallback_label), "%-2.2s", info.label);
    } else {
        info = builtin_stops[state->stopOffset].info;
        strcpy(state->fallback_label, builtin_stops[state->stopOffset].fallback_label);
        result_set = atb_get_next_departures(state->now_ts, builtin_stops[state->stopOffset].route, builtin_stops[state->stopOffset].stop_id);
    }
    strcpy(state->label, info.label);
    strcpy(state->route_label, info.route_label);
    for (int i = 0; i < ATB_COUNTDOWN_DEPARTURES; i++) {
        state->departures[i] = i < MAX_DEPARTURES ? result_set.resultSet[i] : 0;
    }
    state->departures_valid = true;
}

static void draw(atb_countdown_state_t *state, uint8_t subsecond) {
    (void) subsecond;
    char buf[16];

    uint32_t delta;
    div_t result;
    // The timestamp we are passing in should be relative to the current timezone.
    watch_date_time_t now = movement_get_local_date_time();
    state->now_ts = watch_utility_date_time_to_unix_time(now, movement_get_current_timezone_offset());

    // Reading the schedule file is slow, so only look up departures again once
    // the next one has left, or every minute while there are none.
    uint32_t next_ts = state->departures[0];
    if (!state->departures_valid
        || (next_ts != 0 && state->now_ts >= next_ts)
        || (next_ts == 0 && state->now_ts % 60 == 0)) {
        load_departures(state);
    }
    state->target_ts = state->departures[state->offset];

    sprintf(buf, "%-3s", state->label);
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, buf, state->fallback_label);
    sprintf(buf, "%2s", state->route_label);
    watch_display_text_with_fallback(WATCH_POSITION_TOP_RIGHT, buf, buf);

    if (state->target_ts == 0) {
        // No departure in this slot.
        watch_display_text(WATCH_POSITION_BOTTOM, "------");
        return;
    }
    if (state->target_ts <= state->now_ts) {
        delta = 0;
    }
    else {
        delta = state->target_ts - state->now_ts;
    }
    result = div(delta, 60);
    state->seconds = result.rem;
    result = div(result.quot, 60);
    state->hours = result.quot;
    state->minutes = result.rem;
    // Blink to an alternative every now and then.
    if (state->now_ts % 4 != 0) {
        sprintf(buf, "%2d%02d%02d", state->hours, state->minutes, state->seconds);
    }
    else {
        // Convert timestamp to a H:mm situation. We want to display the time of the
        // next departure in the local time, regardless of the timezone. This is
        // mostly so its easier to validate when testing, since these should be the same.
        watch_date_time_t time_to_use = watch_utility_date_time_from_unix_time(state->target_ts, movement_get_current_timezone_offset_for_zone(UTZ_BERLIN));
        sprintf(buf, "%2d%02d%02d", time_to_use.unit.hour, time_to_use.unit.minute, 0);
    }

    watch_display_text(WATCH_POSITION_BOTTOM, buf);
}

void atb_countdown_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;

    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(atb_countdown_state_t));
        atb_countdown_state_t *state = (atb_countdown_state_t *)*context_ptr;
        memset(*context_ptr, 0, sizeof(atb_countdown_state_t));
        state->watch_face_index = watch_face_index;
        state->offset = 0;
        store_countdown(state);
    }
}

void atb_countdown_face_activate(void *context) {
    atb_countdown_state_t *state = (atb_countdown_state_t *)context;
    // The schedule file may have changed while we were away.
    load_stops(state);
    watch_set_colon();

    movement_request_tick_frequency(1);
}

bool atb_countdown_face_loop(movement_event_t event, void *context) {
    atb_countdown_state_t *state = (atb_countdown_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            draw(state, event.subsecond);
            break;
        case EVENT_TICK:
            draw(state, event.subsecond);
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            // Let's not trigger light.
            break;
        case EVENT_LIGHT_BUTTON_UP:
            state->offset = 0;
            if (state->stopOffset + 1 < state->stop_count) {
                state->stopOffset++;
            }
            else {
                state->stopOffset = 0;
            }
            state->departures_valid = false;
            draw(state, event.subsecond);
            break;
        case EVENT_ALARM_BUTTON_UP:
            if (state->offset < 4) {
                state->offset++;
            }
            else {
                state->offset = 0;
            }
            draw(state, event.subsecond);
            break;

        case EVENT_TIMEOUT:
            movement_move_to_face(0);
            break;

        default:
            movement_default_loop_handler(event);
            break;
    }

    return true;
}

void atb_countdown_face_resign(void *context) {
    atb_countdown_state_t *state = (atb_countdown_state_t *)context;
}
