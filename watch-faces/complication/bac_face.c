/*
 * MIT License
 *
 * Copyright (c) 2024 Joey Castillo
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
#include "bac_face.h"
#include "watch.h"
#include "watch_utility.h"

const float r_value = 0.68;
const int weight = 90;

void bac_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(bac_state_t));
        memset(*context_ptr, 0, sizeof(bac_state_t));
    }
}

void bac_face_activate(void *context) {
    printf("bac_face_activate\n");
    printf("state: %d\n", ((bac_state_t *)context)->units);
}

void parseBac(float val, char result[3][8]) {
    char str[32] = {0};
    sprintf(str, "%.6f", val); // Convert float to string with 6 decimals

    // The digit before the dot
    result[0][0] = str[0];
    result[0][1] = '\0';

    // The first and second digits after the dot
    strncpy(result[1], &str[2], 2);
    result[1][2] = '\0';

    // The third and fourth digits after the dot
    strncpy(result[2], &str[4], 2);
    result[2][2] = '\0';
}


void _bac_set_display(void *context) {
    char buf[11];
    // Get the int from the context->units;
    int units = ((bac_state_t *)context)->units;
    // Pad it with space if needed.
    if (units < 10) {
        sprintf(buf, " %d", units);
    } else {
        sprintf(buf, "%d", units);
    }
    // If the start time is more than 0.
    if (((bac_state_t *)context)->start_time > 0) {
        // Standard beer percentage of 4.7.
        float beer_percentage = 4.7;
        // Standard size, .5.
        int size = 500;
        // 500 means 500 grams. Let's find out how many grams of alcohol.
        float alcohol_grams = size * (beer_percentage / 100);
        float total_alcohol_grams = alcohol_grams * units;
        // So the actual algorithm is:
        // BAC = [Alcohol consumed in grams / (Body weight in grams x r)] x 100. In this formula, “r” is the gender constant: r = 0.55 for females and 0.68 for males.
        float bac = (total_alcohol_grams / (weight * 100) * r_value) * 100;
        watch_date_time_t date_time = watch_rtc_get_date_time();
        uint32_t now = watch_utility_date_time_to_unix_time(date_time, 0);
        printf("new now: %d\n", now);
        int total_seconds = (now - ((bac_state_t *)context)->start_time);
        printf("total_seconds: %d\n", total_seconds);
        // Then remove 0.015 per hour since start.
        float total_hours = (total_seconds / 3.6) / 1000;
        printf("eirik total_hours: %f\n", total_hours);
        bac = bac - (0.015 * total_hours);
        printf("bac: %f\n", bac);
        // The BAC will now be something like:
        // 0.007222.
        // I want to split it like this. First segment should be the whole number. I.e 0.
        char result[3][8];
        parseBac(bac, result);
        printf("Integer part: %s\n", result[0]);
        printf("First two decimal places: %s\n", result[1]);
        printf("Next two decimal places: %s\n", result[2]);
        char hour_buf[11];
        char minute_buf[11];
        char second_buf[11];
        sprintf(hour_buf, " %s", result[0]);
        sprintf(minute_buf, "%s", result[1]);
        sprintf(second_buf, "%s", result[2]);
        watch_display_text(WATCH_POSITION_HOURS, hour_buf);
        watch_display_text(WATCH_POSITION_MINUTES, minute_buf);
        watch_display_text(WATCH_POSITION_SECONDS, second_buf);
    }
    else {
        watch_display_text(WATCH_POSITION_HOURS, "00");
        watch_display_text(WATCH_POSITION_MINUTES, "00");
        watch_display_text(WATCH_POSITION_SECONDS, "00");
    }
    // Set the display.
    watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
    watch_set_colon();
}

bool bac_face_loop(movement_event_t event, void *context) {
    (void) context;
    char buf[11];
    switch (event.event_type) {
        case EVENT_ALARM_BUTTON_UP:
          // Increment units.
          ((bac_state_t *)context)->units++;
          // If the start time is not set. Set it to the current timestamp.
          if (((bac_state_t *)context)->start_time == 0) {
                watch_date_time_t date_time = watch_rtc_get_date_time();
                uint32_t now = watch_utility_date_time_to_unix_time(date_time, 0);
                ((bac_state_t *)context)->start_time = now;
          }
          _bac_set_display(context);
          break;

        case EVENT_ACTIVATE:
        case EVENT_TICK:
          watch_display_text_with_fallback(WATCH_POSITION_TOP, "bac", "ac");
          _bac_set_display(context);
          break;

        case EVENT_LIGHT_LONG_PRESS:
            // Reset the counter.
            ((bac_state_t *)context)->units = 0;
            ((bac_state_t *)context)->start_time = 0;
            _bac_set_display(context);
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

void bac_face_resign(void *context) {
    (void) context;
}
