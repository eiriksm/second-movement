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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "day_countdown_face.h"
#include "filesystem.h"
#include "watch.h"

#define DAY_COUNTDOWN_FILE "days.txt"
#define DAY_COUNTDOWN_LINE_LENGTH 48

// Month 0 means Easter Sunday, which moves around.
static const struct {
    char label[4];
    uint8_t month;
    uint8_t day;
} presets[] = {
    {"NEW", 1, 1},
    {"EAS", 0, 0},
    {"17M", 5, 17},
    {"SUM", 6, 23},
    {"HAL", 10, 31},
    {"XMS", 12, 24},
};

#define PRESET_COUNT (sizeof(presets) / sizeof(presets[0]))

// Days since 1970-01-01, from https://howardhinnant.github.io/date_algorithms.html
static int32_t days_from_civil(int32_t y, int32_t m, int32_t d) {
    y -= m <= 2;
    int32_t era = (y >= 0 ? y : y - 399) / 400;
    int32_t yoe = y - era * 400;
    int32_t doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(int32_t z, int32_t *y, uint32_t *m, uint32_t *d) {
    z += 719468;
    int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    uint32_t doe = (uint32_t)(z - era * 146097);
    uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    uint32_t mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = (int32_t)yoe + era * 400 + (*m <= 2);
}

// Anonymous Gregorian algorithm.
static int32_t easter_day(int32_t y) {
    int32_t a = y % 19, b = y / 100, c = y % 100;
    int32_t d = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3;
    int32_t h = (19 * a + b - d - g + 15) % 30;
    int32_t i = c / 4, k = c % 4;
    int32_t l = (32 + 2 * e + 2 * i - h - k) % 7;
    int32_t m = (a + 11 * h + 22 * l) / 451;
    int32_t month = (h + l - 7 * m + 114) / 31;
    int32_t day = (h + l - 7 * m + 114) % 31 + 1;
    return days_from_civil(y, month, day);
}

static int32_t preset_day(uint8_t index, int32_t year) {
    if (presets[index].month == 0) {
        return easter_day(year);
    }
    return days_from_civil(year, presets[index].month, presets[index].day);
}

static int32_t local_today(int32_t *year) {
    watch_date_time_t now = movement_get_local_date_time();
    *year = now.unit.year + WATCH_RTC_REFERENCE_YEAR;
    return days_from_civil(*year, now.unit.month, now.unit.day);
}

static uint8_t counter_count(day_countdown_state_t *state) {
    return state->file_count + PRESET_COUNT;
}

static const char *counter_label(day_countdown_state_t *state, uint8_t index) {
    if (index < state->file_count) {
        return state->counters[index].label;
    }
    return presets[index - state->file_count].label;
}

// The local day the counter is counting towards.
static int32_t counter_day(day_countdown_state_t *state, uint8_t index, int32_t today, int32_t year) {
    if (index < state->file_count) {
        int64_t local_ts = (int64_t)state->counters[index].ts + movement_get_current_timezone_offset();
        return (int32_t)(local_ts >= 0 ? local_ts / 86400 : (local_ts - 86399) / 86400);
    }
    // Presets count down to their next occurrence, including today.
    uint8_t preset = index - state->file_count;
    int32_t day = preset_day(preset, year);
    if (day < today) {
        day = preset_day(preset, year + 1);
    }
    return day;
}

static bool parse_line(char *line, day_countdown_counter_t *counter) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ')) {
        line[--len] = '\0';
    }
    if (len == 0 || line[0] == '#') {
        return false;
    }
    char *separator = strchr(line, '|');
    if (separator == NULL) {
        return false;
    }
    char *end;
    unsigned long ts = strtoul(separator + 1, &end, 10);
    if (end == separator + 1 || *end != '\0') {
        return false;
    }
    *separator = '\0';
    // Only the first three characters fit on the display.
    strncpy(counter->label, line, sizeof(counter->label) - 1);
    counter->label[sizeof(counter->label) - 1] = '\0';
    counter->ts = (uint32_t)ts;
    return true;
}

static void load_counters(day_countdown_state_t *state) {
    state->file_count = 0;
    if (filesystem_file_exists(DAY_COUNTDOWN_FILE)) {
        int32_t size = filesystem_get_file_size(DAY_COUNTDOWN_FILE);
        int32_t offset = 0;
        char line[DAY_COUNTDOWN_LINE_LENGTH];
        // filesystem_read_line does not report the end of the file by itself.
        while (offset < size && state->file_count < DAY_COUNTDOWN_MAX_FILE_COUNTERS) {
            if (!filesystem_read_line(DAY_COUNTDOWN_FILE, line, &offset, sizeof(line) - 1)) {
                break;
            }
            if (parse_line(line, &state->counters[state->file_count])) {
                state->file_count++;
            }
        }
    }
    if (state->selection >= counter_count(state)) {
        state->selection = 0;
    }
}

static void draw(day_countdown_state_t *state) {
    char buf[16];
    int32_t year;
    int32_t today = local_today(&year);
    int32_t day = counter_day(state, state->selection, today, year);
    const char *label = counter_label(state, state->selection);

    snprintf(buf, sizeof(buf), "%-3.3s", label);
    char fallback[3];
    snprintf(fallback, sizeof(fallback), "%-2.2s", label);
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, buf, fallback);
    snprintf(buf, sizeof(buf), "%2d", (state->selection + 1) % 100);
    watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);

    if (state->showing_date) {
        int32_t y;
        uint32_t m, d;
        civil_from_days(day, &y, &m, &d);
        snprintf(buf, sizeof(buf), "%02d%02d%02d", (int)(y % 100), (int)m, (int)d);
    } else {
        int32_t days = day - today;
        if (days > 999999) days = 999999;
        if (days < -99999) days = -99999;
        snprintf(buf, sizeof(buf), "%6d", (int)days);
    }
    watch_display_text(WATCH_POSITION_BOTTOM, buf);
    state->drawn_day = today;
}

void day_countdown_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(day_countdown_state_t));
        memset(*context_ptr, 0, sizeof(day_countdown_state_t));
    }
}

void day_countdown_face_activate(void *context) {
    day_countdown_state_t *state = (day_countdown_state_t *)context;
    // The file may have changed while we were away.
    load_counters(state);
    state->showing_date = false;
}

bool day_countdown_face_loop(movement_event_t event, void *context) {
    day_countdown_state_t *state = (day_countdown_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            draw(state);
            break;
        case EVENT_TICK:
        case EVENT_LOW_ENERGY_UPDATE: {
            // The display only changes at midnight.
            int32_t year;
            if (local_today(&year) != state->drawn_day) {
                draw(state);
            }
            break;
        }
        case EVENT_ALARM_BUTTON_UP:
            state->selection = (state->selection + 1) % counter_count(state);
            draw(state);
            break;
        case EVENT_ALARM_LONG_PRESS:
            state->showing_date = true;
            draw(state);
            break;
        case EVENT_ALARM_LONG_UP:
            state->showing_date = false;
            draw(state);
            break;
        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void day_countdown_face_resign(void *context) {
    day_countdown_state_t *state = (day_countdown_state_t *)context;
    state->showing_date = false;
}
