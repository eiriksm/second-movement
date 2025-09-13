/*
 * MIT License
 *
 * Copyright (c) 2024 Second Movement Project
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
#include "fesk_demo_face.h"
#include "fesk_tx.h"

typedef enum {
    FDM_READY = 0,      // Ready to transmit
    FDM_COUNTDOWN,      // Countdown in progress
    FDM_TRANSMITTING,   // Actively transmitting
} fesk_demo_mode_t;

typedef struct {
    // Current mode
    fesk_demo_mode_t mode;

    // Countdown timer
    uint8_t countdown_ticks;
    uint8_t countdown_seconds;

    // Transmission state
    uint8_t transmission_ticks;
    uint8_t tick_count;

    // FESK encoder state
    fesk_encoder_state_t encoder_state;

    // Buzzer state tracking
    bool buzzer_is_on;

} fesk_demo_state_t;

static char tone_string[1024];

// Test message to transmit
static const uint8_t test_message[] = "test";
static const uint16_t test_message_len = sizeof(test_message) - 1;

void fesk_demo_face_setup(uint8_t watch_face_index, void **context_ptr) {
    (void)watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(fesk_demo_state_t));
        memset(*context_ptr, 0, sizeof(fesk_demo_state_t));
    }
}

void fesk_demo_face_activate(void *context) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)context;

    // Initialize state
    state->mode = FDM_READY;
    state->countdown_ticks = 0;
    state->countdown_seconds = 0;
    state->transmission_ticks = 0;
    state->tick_count = 0;
    state->buzzer_is_on = false;
}

static void _fdf_update_display(fesk_demo_state_t *state) {
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "FK", "FESK");

    switch (state->mode) {
        case FDM_READY:
            watch_display_text(WATCH_POSITION_BOTTOM, " TEST ");
            break;

        case FDM_COUNTDOWN:
            watch_display_text(WATCH_POSITION_BOTTOM, "     3");
            if (state->countdown_seconds == 2) {
                watch_display_text(WATCH_POSITION_BOTTOM, "     2");
            } else if (state->countdown_seconds == 1) {
                watch_display_text(WATCH_POSITION_BOTTOM, "     1");
            } else if (state->countdown_seconds == 0) {
                watch_display_text(WATCH_POSITION_BOTTOM, "    GO");
            }
            break;

        case FDM_TRANSMITTING:
            watch_display_text(WATCH_POSITION_BOTTOM, "  TX  ");
            break;
    }
}

static void _fdf_start_countdown(fesk_demo_state_t *state) {
    state->mode = FDM_COUNTDOWN;
    state->countdown_seconds = 3;
    state->countdown_ticks = 0;

    // Request 64Hz ticks for countdown
    movement_request_tick_frequency(64);

    // Show bell indicator during transmission
    watch_set_indicator(WATCH_INDICATOR_BELL);

    _fdf_update_display(state);
    tone_string[0] = '\0';  // Clear the tone string
}

static void _fdf_stop_transmission(fesk_demo_state_t *state);

static void _fdf_start_transmission(fesk_demo_state_t *state) {
    state->mode = FDM_TRANSMITTING;
    state->transmission_ticks = 0;
    state->tick_count = 0;

    // Initialize FESK encoder with test message
    fesk_result_t result = fesk_init_encoder(&state->encoder_state,
                                             test_message,
                                             test_message_len);

    if (result != FESK_SUCCESS) {
        // Error - go back to ready
        _fdf_stop_transmission(state);
        return;
    }

    // Turn on buzzer at start of transmission
    watch_set_buzzer_on();
    state->buzzer_is_on = true;

    _fdf_update_display(state);
}

static void _fdf_stop_transmission(fesk_demo_state_t *state) {
    state->mode = FDM_READY;

    // Stop buzzer and clear indicators
    watch_set_buzzer_off();
    state->buzzer_is_on = false;
    watch_clear_indicator(WATCH_INDICATOR_BELL);

    // Return to 1Hz ticks
    movement_request_tick_frequency(1);

    _fdf_update_display(state);
}

static void _fdf_handle_countdown_tick(fesk_demo_state_t *state) {
    state->countdown_ticks++;

    // 64 ticks per second, so every 64 ticks = 1 second
    if (state->countdown_ticks >= 64) {
        state->countdown_ticks = 0;

        if (state->countdown_seconds > 0) {
            state->countdown_seconds--;

            // Play countdown beep
            watch_set_buzzer_period_and_duty_cycle(NotePeriods[BUZZER_NOTE_A5], 25);
            watch_set_buzzer_on();

            // Turn off beep after a short time (will happen on next tick)

        } else {
            // Countdown finished - start transmission
            _fdf_start_transmission(state);
            return;
        }

        _fdf_update_display(state);

    } else if (state->countdown_ticks == 8) {
        // Turn off countdown beep after ~1/8 second
        watch_set_buzzer_off();
    }
}

// A global var to hold a comma separated string of the tones we emit.

static void _fdf_handle_transmission_tick(fesk_demo_state_t *state) {
    state->tick_count++;

    // Symbol duration is configurable, default is 6 ticks (93.75ms at 64Hz)
    uint8_t symbol_ticks = state->encoder_state.config.symbol_ticks;

    if (state->tick_count >= symbol_ticks) {
        state->tick_count = 0;

        // Get next tone from encoder
        uint8_t tone = fesk_get_next_tone(&state->encoder_state);
        // Append the variable "tone" to the tone_string
        snprintf(tone_string + strlen(tone_string), sizeof(tone_string) - strlen(tone_string), ",%d", tone);

        if (tone == 255) {
            // Transmission complete
            _fdf_stop_transmission(state);
            printf("Tones emitted: %s\n", tone_string);
            return;
        }

        // Set buzzer to the tone frequency
        uint16_t period = fesk_get_tone_period(&state->encoder_state, tone);
        if (period > 0) {
            //printf("Tone: %d, Period: %d\n", tone, period);
            watch_set_buzzer_period_and_duty_cycle(period, 25);
            watch_set_buzzer_on();
        }
    }
}

bool fesk_demo_face_loop(movement_event_t event, void *context) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            _fdf_update_display(state);
            break;

        case EVENT_MODE_BUTTON_UP:
            // Don't exit while transmitting or counting down
            if (state->mode == FDM_READY) {
                movement_move_to_next_face();
            }
            break;

        case EVENT_LIGHT_BUTTON_UP:
            // Light button does nothing in this face
            break;

        case EVENT_ALARM_BUTTON_UP:
            if (state->mode == FDM_READY) {
                // Start countdown
                _fdf_start_countdown(state);
            } else if (state->mode == FDM_COUNTDOWN || state->mode == FDM_TRANSMITTING) {
                // Cancel countdown/transmission
                _fdf_stop_transmission(state);
            }
            break;

        case EVENT_ALARM_LONG_PRESS:
            // Same behavior as short press for simplicity
            break;

        case EVENT_TICK:
            if (state->mode == FDM_COUNTDOWN) {
                _fdf_handle_countdown_tick(state);
            } else if (state->mode == FDM_TRANSMITTING) {
                _fdf_handle_transmission_tick(state);
            }
            break;

        case EVENT_TIMEOUT:
            // Don't timeout while active
            if (state->mode == FDM_READY) {
                movement_move_to_face(0);
            }
            break;

        default:
            break;
    }

    // Return false during active operations to prevent sleep
    return (state->mode == FDM_READY);
}

void fesk_demo_face_resign(void *context) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)context;

    if (state && (state->mode == FDM_COUNTDOWN || state->mode == FDM_TRANSMITTING)) {
        _fdf_stop_transmission(state);
    }
}
