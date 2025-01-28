#include <stdlib.h>
#include <string.h>
#include "beer_counter_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "bac.h"

#define ELIMINATION_RATE_H 0.15f // Average elimination rate (g/kg/hour)
#define ALCOHOL_DENSITY 0.789  // g/ml

void beer_counter_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(beer_counter_state_t));
        beer_counter_state_t *state = (beer_counter_state_t *)*context_ptr;
        memset(*context_ptr, 0, sizeof(beer_counter_state_t));
        state->beer_count = 0;
        state->weight = 95;
        state->sex = 0;
        state->edit_on = false;
    }
}

static void print_unit_count(beer_counter_state_t *state) {
    char buf[4];
    // We actually do not want to pad with 0 but with space.
    sprintf(buf, "%2d", state->beer_count);
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "BC", "BC");
    watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
    // Let's also get the BAC.
    float bac = calculate_bac(state);
    char result[3][8];
    parse_bac_into_result(bac, result);
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

void parse_bac_into_result(float val, char result[3][8]) {
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

static float calculate_bac(beer_counter_state_t *state) {
    float result;
    float alcohol_g = 0.0;
    watch_date_time_t now = movement_get_local_date_time();
    uint32_t current_time_unix = watch_utility_date_time_to_unix_time(now, movement_get_current_timezone_offset());
    uint32_t time_elapsed_seconds = current_time_unix - state->start_time;
    if (time_elapsed_seconds < 0) {
        time_elapsed_seconds = 0;
    }
    if (state->beer_count == 0) {
        return 0;
    }
    for (int i = 0; i < state->beer_count; i++) {
        float unit_g = (state->units[i].volume * (state->units[i].percentage / 10) * ALCOHOL_DENSITY) / 100;
        alcohol_g += unit_g;
    }
    if (state->sex == 1) {
        result = bac_for_women_from_weight_and_alcohol_grams(state->weight, alcohol_g, state->start_time, current_time_unix);
    }
    else {
        result = bac_for_men_from_weight_and_alcohol_grams(state->weight, alcohol_g, state->start_time, current_time_unix);
    }
    return result;
}

void beer_counter_face_activate(void *context) {
    movement_request_tick_frequency(4);
    watch_set_led_off();

    beer_counter_state_t *state = (beer_counter_state_t *)context;

    float bac = calculate_bac(state);
    if (bac == 0) {
        state->beer_count = 0;
    }
    print_unit_count(state);
}

void draw_screen(beer_counter_state_t *state) {
    if (state->screen_delta == 0) {
        print_unit_count(state);
    }
    if (state->screen_delta == 1) {
        print_edit_screen(state);
    }
}

void print_edit_screen(beer_counter_state_t *state) {
    char buf[10];
    int delta = state->beer_count - state->edit_offset;
    sprintf(buf, "%2d", delta);
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "ED", "EDT");
    watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
    // There is one value to display, and another to talk about to the computer.
    int computer_delta = delta - 1;
    int volume = state->units[computer_delta].volume;
    int percentage = state->units[computer_delta].percentage;
    // Depending on edit mode or not, we want to display different things.
    if (state->edit_on) {
        // Check if we are editing volume or percentage.
        if (state->is_alc_cont_screen) {
            // Blank it out first.
            int tens = percentage * 10;
            watch_display_text(WATCH_POSITION_HOURS, "  ");
            watch_display_text(WATCH_POSITION_MINUTES, "  ");
            watch_display_text(WATCH_POSITION_SECONDS, "  ");
            sprintf(buf, "%d", tens % 100);
            watch_display_text(WATCH_POSITION_SECONDS, buf);
            sprintf(buf, "%2d", (tens - (tens % 100) ) / 100);
            watch_display_text(WATCH_POSITION_MINUTES, buf);
        }
        else {
            sprintf(buf, "%d", volume);
            watch_display_text(WATCH_POSITION_BOTTOM, buf);
        }
        // Blink it a bit.
        watch_date_time_t now = movement_get_local_date_time();
        uint32_t current_time_unix = watch_utility_date_time_to_unix_time(now, movement_get_current_timezone_offset());
        if (current_time_unix % 2 == 0) {
            watch_display_text(WATCH_POSITION_HOURS, "  ");
            watch_display_text(WATCH_POSITION_MINUTES, "  ");
            watch_display_text(WATCH_POSITION_SECONDS, "  ");
        }
    }
    else {
        sprintf(buf, "%d", volume);
        // Split the volume in two parts. First part should be the first number.
        char first_part[3];
        sprintf(first_part, " %d", volume / 100);
        watch_display_text(WATCH_POSITION_HOURS, first_part);

        char second_part[3];
        second_part[0] = buf[1];
        second_part[1] = buf[2];
        second_part[2] = '\0';
        watch_display_text(WATCH_POSITION_MINUTES, second_part);
        sprintf(buf, "%d", percentage);
        watch_display_text(WATCH_POSITION_SECONDS, buf);
    }
}

bool beer_counter_face_loop(movement_event_t event, void *context) {
    beer_counter_state_t *state = (beer_counter_state_t *)context;

    switch (event.event_type) {
        case EVENT_TICK:
            draw_screen(state);
            break;

        case EVENT_ACTIVATE:
            state->screen_delta = 0;
            state->edit_on = false;
            state->edit_offset = 0;
            print_unit_count(state);
            break;

        case EVENT_ALARM_BUTTON_UP:
            // If we are on the edit screen, we want to change the value.
            if (state->screen_delta == 1) {
                if (state->edit_on) {
                    int delta = state->beer_count - state->edit_offset;
                    int computer_delta = delta - 1;
                    if (state->is_alc_cont_screen) {
                        state->units[computer_delta].percentage += 5;
                    }
                    else {
                        // Cycle through the available volumes.
                        if (state->units[computer_delta].volume == 300) {
                            state->units[computer_delta].volume = 330;
                        }
                        else if (state->units[computer_delta].volume == 330) {
                            state->units[computer_delta].volume = 400;
                        }
                        else if (state->units[computer_delta].volume == 400) {
                            state->units[computer_delta].volume = 500;
                        }
                        else if (state->units[computer_delta].volume == 500) {
                            state->units[computer_delta].volume = 600;
                        }
                        else if (state->units[computer_delta].volume == 600) {
                            state->units[computer_delta].volume = 700;
                        }
                        else if (state->units[computer_delta].volume == 700) {
                            state->units[computer_delta].volume = 750;
                        }
                        else if (state->units[computer_delta].volume == 750) {
                            state->units[computer_delta].volume = 300;
                        }
                    }
                }
                else {
                    // Cycle the units.
                    state->edit_offset++;
                    if (state->edit_offset >= state->beer_count) {
                        state->edit_offset = 0;
                    }
                }
                print_edit_screen(state);
                break;
            }
            // Append one more item to the units array.
            state->beer_count++;
            state->units[state->beer_count - 1].volume = 500;
            state->units[state->beer_count - 1].percentage = 45;
            // If the start time is not set, set it to the current timestamp.
            if (state->start_time == 0) {
                watch_date_time_t date_time = movement_get_local_date_time();
                state->start_time = watch_utility_date_time_to_unix_time(date_time, movement_get_current_timezone_offset());
            }
            print_unit_count(state);
            break;

        case EVENT_LIGHT_BUTTON_UP:
            // If the count is 0, then do nothing.
            if (state->beer_count == 0) {
                break;
            }
            // Cycle the modes. Unless in edit mode.
            if (state->screen_delta == 1 && state->edit_on) {
                if (!state->is_alc_cont_screen) {
                    state->is_alc_cont_screen = true;
                }
                else {
                    state->is_alc_cont_screen = false;
                    state->edit_offset++;
                }
                if (state->edit_offset >= state->beer_count) {
                    state->edit_offset = 0;
                }
                print_edit_screen(state);
                break;
            }
            state->screen_delta++;
            if (state->screen_delta > 1) {
                state->screen_delta = 0;
            }
            draw_screen(state);
            break;

        case EVENT_LIGHT_LONG_PRESS:
            // Reset. Unless in edit mode.
            if (state->screen_delta == 1) {
                state->edit_on = !state->edit_on;
                if (state->edit_on) {
                    state->edit_offset = 0;
                    state->is_alc_cont_screen = false;
                }
                print_edit_screen(state);
                break;
            }
            state->beer_count = 0;
            state->start_time = 0;
            print_unit_count(state);
            break;

        case EVENT_TIMEOUT:
            movement_move_to_face(0);
            break;


        default:
            return movement_default_loop_handler(event);
    }
    return true;

}

void beer_counter_face_resign(void *context) {
}

static uint32_t calculate_time_to_sober(float current_bac) {
    float time_to_sober_hours = current_bac / ELIMINATION_RATE_H;
    uint32_t time_to_sober_seconds = (uint32_t)(time_to_sober_hours * 3600);
    return time_to_sober_seconds;
}
