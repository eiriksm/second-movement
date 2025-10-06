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
#include <stdio.h>
#include "fesk_demo_face.h"
#include "fesk_tx.h"
#include "movement.h"

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

    // Buzzer state tracking
    bool buzzer_is_on;

    bool is_playing_sequence;

    // FESK sequence for buzzer playback
    int8_t *fesk_sequence;
    size_t fesk_sequence_length;

} fesk_demo_state_t;

static char tone_string[1024];

// Test message to transmit
static const char test_message[] = "a fairly long, and might i say convoluted test message: yes";
static const size_t test_message_len = sizeof(test_message) - 1;

// Global callback state for sequence completion
static fesk_demo_state_t *melody_callback_state = NULL;

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
    state->is_playing_sequence = false;
    state->fesk_sequence = NULL;
    state->fesk_sequence_length = 0;
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
static void _fdf_fesk_transmission_done(void);

static bool _fdf_build_fesk_sequence(fesk_demo_state_t *state) {
    int8_t *sequence = NULL;
    size_t entries = 0;
    fesk_result_t result = fesk_encode_text(test_message, test_message_len, &sequence, &entries);
    if (result != FESK_OK) {
        return false;
    }

    state->fesk_sequence = sequence;
    state->fesk_sequence_length = entries;
    return true;
}

static void _fdf_start_transmission(fesk_demo_state_t *state) {
    state->mode = FDM_TRANSMITTING;
    state->transmission_ticks = 0;
    state->tick_count = 0;

    // Build FESK sequence
    if (state->fesk_sequence) {
        fesk_free_sequence(state->fesk_sequence);
        state->fesk_sequence = NULL;
    }

    if (!_fdf_build_fesk_sequence(state)) {
        // Error - go back to ready
        _fdf_stop_transmission(state);
        return;
    }

    // Start playing the sequence
    state->is_playing_sequence = true;
    melody_callback_state = state;
    watch_buzzer_play_sequence(state->fesk_sequence, _fdf_fesk_transmission_done);

    _fdf_update_display(state);
}

static void _fdf_fesk_transmission_done(void) {
    if (melody_callback_state) {
        melody_callback_state->is_playing_sequence = false;
        fesk_demo_state_t *state = melody_callback_state;
        melody_callback_state = NULL;  // Clear callback state to prevent re-entry
        _fdf_stop_transmission(state);
        printf("FESK transmission complete\n");
    }
}

static void _fdf_stop_transmission(fesk_demo_state_t *state) {
    state->mode = FDM_READY;

    // Stop any playing sequence
    if (state->is_playing_sequence) {
        watch_buzzer_abort_sequence();
        state->is_playing_sequence = false;
    }

    // Free sequence memory
    if (state->fesk_sequence) {
        fesk_free_sequence(state->fesk_sequence);
        state->fesk_sequence = NULL;
        state->fesk_sequence_length = 0;
    }

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


static int8_t game_win_melody[] = {
    BUZZER_NOTE_G6, 6,
    BUZZER_NOTE_A6, 6,
    BUZZER_NOTE_B6, 6,
    BUZZER_NOTE_C7, 6,
    BUZZER_NOTE_D7, 6,
    BUZZER_NOTE_E7, 6,
    BUZZER_NOTE_D7, 6,
    BUZZER_NOTE_C7, 6,
    BUZZER_NOTE_B6, 6,
    BUZZER_NOTE_C7, 6,
    BUZZER_NOTE_D7, 6,
    BUZZER_NOTE_G7, 6,
    0};

void fesk_demo_face_melody_done(void) {
    if (melody_callback_state) {
        melody_callback_state->is_playing_sequence = false;
        printf("Melody done\n");
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

            // Play a melody.
            if (state->is_playing_sequence) {
                // Already playing a sequence, ignore
                break;
            }
            state->is_playing_sequence = true;
            melody_callback_state = state;
            watch_buzzer_play_sequence(game_win_melody, fesk_demo_face_melody_done);
            break;

        case EVENT_TICK:
            if (state->mode == FDM_COUNTDOWN) {
                _fdf_handle_countdown_tick(state);
            }
            // Note: FDM_TRANSMITTING now uses watch_buzzer_play_sequence, no tick handling needed
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
    return (state->mode == FDM_READY && !state->is_playing_sequence);
}

void fesk_demo_face_resign(void *context) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)context;

    if (state && (state->mode == FDM_COUNTDOWN || state->mode == FDM_TRANSMITTING)) {
        _fdf_stop_transmission(state);
    }
}
