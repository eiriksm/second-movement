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

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include "uptime_face.h"
#include "fesk_tx.h"
#include "watch.h"
#include "watch_utility.h"
#define U32_DEC_DIGITS 10
#define UPTIME_BUFSZ (7 + U32_DEC_DIGITS + 8 + 1) // "uptime " + digits + " seconds" + '\0'

// persistent buffer (don't point globals at a local/stack buffer)
static uint8_t long_data_str[UPTIME_BUFSZ];


void uptime_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(uptime_state_t));
        memset(*context_ptr, 0, sizeof(uptime_state_t));
        uptime_state_t *state = (uptime_state_t *)*context_ptr;
        watch_date_time_t now = movement_get_local_date_time();
        state->boot_time = watch_utility_date_time_to_unix_time(now, movement_get_current_timezone_offset());
    }
}

void uptime_face_activate(void *context) {
    uptime_state_t *state = (uptime_state_t *)context;
}

static void _uptime_quit_chirping(uptime_state_t *state) {
    state->mode = UT_NONE;
    watch_set_buzzer_off();
    watch_clear_indicator(WATCH_INDICATOR_BELL);
    movement_request_tick_frequency(1);
}

static void _uptime_data_tick(void *context) {
    uptime_state_t *state = (uptime_state_t *)context;

    uint8_t tone = fesk_get_next_tone(&state->encoder_state);
    // Transmission over?
    if (tone == 255) {
        _uptime_quit_chirping(state);
        return;
    }
    uint16_t period = fesk_get_tone_period(&state->encoder_state, tone);
    watch_set_buzzer_period_and_duty_cycle(period, 25);
    watch_set_buzzer_on();
}


static inline size_t build_uptime_line(uint32_t seconds_since_boot)
{
    // no newline at the end -> avoids the blank console line
    int n = snprintf((char *)long_data_str, sizeof long_data_str,
                     "uptime %" PRIu32 " seconds", seconds_since_boot);

    // normalize/clamp
    if (n < 0) return 0;
    if ((size_t)n >= sizeof long_data_str) n = (int)sizeof long_data_str - 1;
    return (size_t)n; // number of meaningful bytes (excludes NUL)
}

static uint32_t uptime_get_seconds_since_boot(uptime_state_t *state) {
    if (state == NULL) return 0;
    uint32_t now = watch_utility_date_time_to_unix_time(movement_get_local_date_time(), movement_get_current_timezone_offset());
    return now - state->boot_time;
}

static void _uptime_countdown_tick(void *context) {
    uptime_state_t *state = (uptime_state_t *)context;

    ++state->tick_count;

    // Check for second boundary (64 ticks = 1 second at 64Hz)
    if (state->tick_count >= 64) {
        state->tick_count = 0;
        
        // After countdown finishes, start transmission
        if (state->countdown_seconds == 0) {
            // Create uptime string
            uint32_t seconds_since_boot = uptime_get_seconds_since_boot(state);
            size_t len = build_uptime_line(seconds_since_boot);
            
            // Initialize fesk encoder with the uptime data
            fesk_result_t result = fesk_init_encoder(&state->encoder_state, long_data_str, len);
            if (result != FESK_SUCCESS) {
                _uptime_quit_chirping(state);
                return;
            }
            
            // Switch to transmission mode
            state->tick_compare = state->encoder_state.config.symbol_ticks;
            state->tick_count = 0;
            return;
        }
        state->countdown_seconds--;
        
        // Play countdown beep
        watch_set_buzzer_period_and_duty_cycle(NotePeriods[BUZZER_NOTE_A5], 25);
        watch_set_buzzer_on();
    } else if (state->tick_count == 8) {
        // Turn off countdown beep after ~1/8 second
        watch_set_buzzer_off();
    }
}

static void _uptime_setup_chirp(uptime_state_t *state) {
    // We want frequent callbacks from now on
    movement_request_tick_frequency(64); // 64Hz for fesk
    watch_set_indicator(WATCH_INDICATOR_BELL);
    state->mode = UT_CHIRPING;
    // Set up tick state; start with countdown.
    state->tick_count = 0;
    state->tick_compare = 64; // 64 ticks = 1 second for countdown
    state->countdown_seconds = 3; // 3 second countdown
}

bool uptime_face_loop(movement_event_t event, void *context) {
    uptime_state_t *state = (uptime_state_t *)context;


    switch (event.event_type) {
        case EVENT_ACTIVATE:
            // Just display the word "uptime" here.
            watch_display_text_with_fallback(WATCH_POSITION_TOP, "UPTMe", "UP");
            break;
        case EVENT_TICK:
            if (state->mode == UT_CHIRPING) {
                if (fesk_is_transmitting(&state->encoder_state)) {
                    // Handle transmission ticks
                    ++state->tick_count;
                    if (state->tick_count >= state->tick_compare) {
                        state->tick_count = 0;
                        _uptime_data_tick(context);
                    }
                } else {
                    // Handle countdown
                    _uptime_countdown_tick(context);
                }
            } else {
                // Update with seconds on the bottom there.
                char buf[16];
                uint32_t seconds_since_boot = uptime_get_seconds_since_boot(state);
                snprintf(buf, sizeof(buf), "%d", seconds_since_boot);
                watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, buf, "0");
            }
            break;
        case EVENT_LIGHT_BUTTON_UP:
            break;
        case EVENT_ALARM_BUTTON_UP:
            _uptime_setup_chirp(state);
            break;
        case EVENT_TIMEOUT:
            movement_move_to_face(0);
            break;
        case EVENT_LOW_ENERGY_UPDATE:
            break;
        default:
            return movement_default_loop_handler(event);
    }
    // Return true if the watch can enter standby mode. False needed when chirping.
    if (state->mode == UT_CHIRPING) {
        return false;
    }
    return true;
}

void uptime_face_resign(void *context) {
    (void) context;
}

