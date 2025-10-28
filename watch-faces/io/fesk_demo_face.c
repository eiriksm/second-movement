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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fesk_demo_face.h"
#include "fesk_session.h"
#include "movement.h"
#include "watch.h"
#include "watch_tcc.h"

typedef struct {
    fesk_session_t session;
    fesk_session_config_t config;
    bool is_countdown;
    bool is_transmitting;
    bool is_debug_playing;
} fesk_demo_state_t;

static const char test_message[] = "test";
static const size_t test_message_len = sizeof(test_message) - 1;

static fesk_demo_state_t *melody_callback_state = NULL;

static void _demo_display_ready(fesk_demo_state_t *state) {
    (void)state;
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "FK", "FESK");
    watch_display_text(WATCH_POSITION_BOTTOM, " TEST ");
}

static void _demo_update_countdown_display(uint8_t seconds_remaining) {
    char buffer[7] = "      ";
    if (seconds_remaining > 0) {
        snprintf(buffer, sizeof(buffer), "     %u", (unsigned int)seconds_remaining);
    } else {
        strncpy(buffer, "    GO", sizeof(buffer));
    }
    buffer[6] = '\0';
    watch_display_text(WATCH_POSITION_BOTTOM, buffer);
}

static void _demo_on_ready(void *user_data) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)user_data;
    if (!state) return;
    state->is_countdown = false;
    state->is_transmitting = false;
    _demo_display_ready(state);
}

static void _demo_on_countdown_begin(void *user_data) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)user_data;
    if (!state) return;
    state->is_debug_playing = false;
    state->is_countdown = true;
}

static void _demo_on_countdown_tick(uint8_t seconds_remaining, void *user_data) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)user_data;
    if (!state) return;
    state->is_countdown = true;
    _demo_update_countdown_display(seconds_remaining);
}

static void _demo_on_countdown_complete(void *user_data) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)user_data;
    if (!state) return;
    state->is_countdown = false;
}

static void _demo_on_transmission_start(void *user_data) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)user_data;
    if (!state) return;
    state->is_transmitting = true;
    watch_display_text(WATCH_POSITION_BOTTOM, "  TX  ");
}

static void _demo_on_transmission_end(void *user_data) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)user_data;
    if (!state) return;
    state->is_transmitting = false;
    _demo_display_ready(state);
}

static void _demo_on_cancelled(void *user_data) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)user_data;
    if (!state) return;
    state->is_countdown = false;
    state->is_transmitting = false;
    _demo_display_ready(state);
}

static void _demo_on_error(fesk_result_t error, void *user_data) {
    (void)user_data;
    printf("FESK error: %d\n", (int)error);
}

static int8_t debug_sequence[] = {
    BUZZER_NOTE_D7SHARP_E7FLAT, 40,
    BUZZER_NOTE_G7, 40,
    BUZZER_NOTE_D7SHARP_E7FLAT, 40,
    BUZZER_NOTE_G7, 40,
    0
};

static void _demo_debug_done(void) {
    if (melody_callback_state) {
        melody_callback_state->is_debug_playing = false;
        melody_callback_state = NULL;
    }
}

void fesk_demo_face_setup(uint8_t watch_face_index, void **context_ptr) {
    (void)watch_face_index;
    if (*context_ptr != NULL) {
        return;
    }

    fesk_demo_state_t *state = malloc(sizeof(fesk_demo_state_t));
    if (!state) {
        return;
    }
    memset(state, 0, sizeof(*state));

    state->config.enable_countdown = true;
    state->config.countdown_seconds = 3;
    state->config.countdown_beep = true;
    state->config.show_bell_indicator = true;
    state->config.static_message = test_message;
    state->config.static_message_length = test_message_len;
    state->config.on_ready = _demo_on_ready;
    state->config.on_countdown_begin = _demo_on_countdown_begin;
    state->config.on_countdown_tick = _demo_on_countdown_tick;
    state->config.on_countdown_complete = _demo_on_countdown_complete;
    state->config.on_transmission_start = _demo_on_transmission_start;
    state->config.on_transmission_end = _demo_on_transmission_end;
    state->config.on_cancelled = _demo_on_cancelled;
    state->config.on_error = _demo_on_error;
    state->config.user_data = state;

    fesk_session_init(&state->session, &state->config);
    *context_ptr = state;
}

void fesk_demo_face_activate(void *context) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)context;
    if (!state) return;
    state->is_debug_playing = false;
    fesk_session_prepare(&state->session);
}

bool fesk_demo_face_loop(movement_event_t event, void *context) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)context;
    if (!state) return true;

    bool handled = false;

    switch (event.event_type) {
        case EVENT_MODE_BUTTON_UP:
            if (!state->is_debug_playing && fesk_session_is_idle(&state->session)) {
                movement_move_to_next_face();
                handled = true;
            }
            break;

        case EVENT_ALARM_BUTTON_UP:
            if (state->is_debug_playing) {
                handled = true;
                break;
            }
            if (fesk_session_is_idle(&state->session)) {
                if (!state->is_countdown && !state->is_transmitting) {
                    fesk_session_start(&state->session);
                }
            } else {
                fesk_session_cancel(&state->session);
            }
            handled = true;
            break;

        case EVENT_ALARM_LONG_PRESS:
            if (!state->is_debug_playing && !state->is_countdown && !state->is_transmitting) {
                state->is_debug_playing = true;
                melody_callback_state = state;
                watch_buzzer_play_sequence(debug_sequence, _demo_debug_done);
            }
            handled = true;
            break;

        case EVENT_TIMEOUT:
            if (fesk_session_is_idle(&state->session) && !state->is_debug_playing) {
                movement_move_to_face(0);
            }
            handled = true;
            break;

        default:
            break;
    }

    if (!handled) {
        movement_default_loop_handler(event);
    }

    if (state->is_debug_playing) {
        return false;
    }

    return fesk_session_is_idle(&state->session);
}

void fesk_demo_face_resign(void *context) {
    fesk_demo_state_t *state = (fesk_demo_state_t *)context;
    if (!state) return;

    if (state->is_debug_playing) {
        watch_buzzer_abort_sequence();
        state->is_debug_playing = false;
        melody_callback_state = NULL;
    }

    fesk_session_cancel(&state->session);
}
