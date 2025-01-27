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
    }
}

static void print_beer_count(beer_counter_state_t *state) {
    char buf[4];
    // We actually do not want to pad with 0 but with space.
    sprintf(buf, "%2d", state->beer_count);
    printf("Beer count: %s\n", buf);
    printf("beer count from state %d\n", state->beer_count);
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "BC", "BC");
    watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
    // Let's also get the BAC.
    float bac = calculate_bac(state);
    printf("BAC: %f\n", bac);
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
    watch_date_time_t now = movement_get_local_date_time();
    uint32_t current_time_unix = watch_utility_date_time_to_unix_time(now, movement_get_current_timezone_offset());
    uint32_t time_elapsed_seconds = current_time_unix - state->start_time;
    if (time_elapsed_seconds < 0) {
        time_elapsed_seconds = 0;
    }
    if (state->beer_count == 0) {
        return 0;
    }
    printf("time_elapsed_seconds: %d\n", time_elapsed_seconds);

    float alcohol_g = (state->beer_count * 500 * 4.7 * ALCOHOL_DENSITY)/100; //because we put in drink_vol as integers we have to divide by 100
    printf("alcohol_g: %f\n", alcohol_g);
    if (state->sex == 1) {
        result = bac_for_women_from_weight_and_alcohol_grams(state->weight, alcohol_g, state->start_time, current_time_unix);
    }
    else {
        result = bac_for_men_from_weight_and_alcohol_grams(state->weight, alcohol_g, state->start_time, current_time_unix);
    }
    printf("result: %f\n", result);
    return result;
}

void beer_counter_face_activate(void *context) {
    movement_request_tick_frequency(4);
    watch_set_led_off();

    beer_counter_state_t *state = (beer_counter_state_t *)context;

    float bac = calculate_bac(state);
    if (bac == 0) {
        state->beer_count = 0;
        state->old_bac = 0;
    }
    print_beer_count(state);
}

bool beer_counter_face_loop(movement_event_t event, void *context) {
    beer_counter_state_t *state = (beer_counter_state_t *)context;

    switch (event.event_type) {
        case EVENT_TICK:
            print_beer_count(state);
            break;

        case EVENT_ACTIVATE:
            state->is_weight_screen = false;
            state->is_sex_screen = false;
            state->is_bac_screen = false;
            state->is_sober_screen = false;
            state->is_drink_vol_screen = false;
            state->is_alc_cont_screen =false;
            print_beer_count(state);
            break;

        case EVENT_ALARM_BUTTON_UP:
            state->beer_count++;
            // If the start time is not set, set it to the current timestamp.
            if (state->start_time == 0) {
                watch_date_time_t date_time = movement_get_local_date_time();
                state->start_time = watch_utility_date_time_to_unix_time(date_time, movement_get_current_timezone_offset());
            }
            print_beer_count(state);
            break;

        case EVENT_LIGHT_LONG_PRESS:
            // Reset.
            state->beer_count = 0;
            state->start_time = 0;
            print_beer_count(state);
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
