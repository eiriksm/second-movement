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
#include "pentatonic_tx_face.h"
#include "pentatonic_tx.h"
#include "filesystem.h"

typedef enum {
    PTX_MODE_SELECT = 0,
    PTX_MODE_CONFIG,
    PTX_MODE_COUNTDOWN,
    PTX_MODE_TRANSMITTING,
    PTX_MODE_COMPLETE
} ptx_mode_t;

typedef enum {
    PTX_DATA_DEMO = 0,
    PTX_DATA_TIME,
    PTX_DATA_ACTIVITY,
    PTX_DATA_CUSTOM,
    PTX_DATA_COUNT
} ptx_data_source_t;

typedef struct {
    ptx_mode_t mode;
    ptx_data_source_t data_source;
    penta_reliability_level_t reliability_level;

    // Transmission state
    penta_encoder_state_t encoder;
    uint8_t tick_count;
    uint8_t tick_divisor;
    uint8_t current_tone;
    uint16_t tone_timer;

    // Data buffers
    uint8_t *data_buffer;
    uint16_t data_length;
    uint16_t data_pos;

    // Statistics display
    bool show_stats;
    uint8_t stats_page;

    // Countdown state
    uint8_t countdown_phase;

} pentatonic_tx_state_t;

// Demo data - a short message
static const char demo_message[] = "one 2 three";

// Custom file name for user data
#define PTX_CUSTOM_FILE "ptx_data.bin"

// Forward declarations
static void ptx_start_transmission(pentatonic_tx_state_t *state);

// Data source functions
static uint8_t ptx_get_demo_byte(uint8_t *next_byte);
static uint8_t ptx_get_time_byte(uint8_t *next_byte);
static uint8_t ptx_get_activity_byte(uint8_t *next_byte);
static uint8_t ptx_get_custom_byte(uint8_t *next_byte);

// Global pointer to current state (for data callbacks)
static pentatonic_tx_state_t *g_ptx_state = NULL;

void pentatonic_tx_face_setup(uint8_t watch_face_index, void **context_ptr) {
    (void)watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(pentatonic_tx_state_t));
        memset(*context_ptr, 0, sizeof(pentatonic_tx_state_t));
    }
}

void pentatonic_tx_face_activate(void *context) {
    pentatonic_tx_state_t *state = (pentatonic_tx_state_t *)context;

    // Reset state
    state->mode = PTX_MODE_SELECT;
    state->data_source = PTX_DATA_DEMO;
    state->reliability_level = PENTA_BALANCED;
    state->show_stats = false;
    state->stats_page = 0;

    // Clean up any previous data buffer
    if (state->data_buffer) {
        free(state->data_buffer);
        state->data_buffer = NULL;
        state->data_length = 0;
    }

    g_ptx_state = state;
}

static void ptx_update_display(pentatonic_tx_state_t *state) {
    switch (state->mode) {
        case PTX_MODE_SELECT:
            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "PT", "Penta");

            // Show data source
            switch (state->data_source) {
                case PTX_DATA_DEMO:
                    watch_display_text(WATCH_POSITION_BOTTOM, "DEMO  ");
                    break;
                case PTX_DATA_TIME:
                    watch_display_text(WATCH_POSITION_BOTTOM, "TIME  ");
                    break;
                case PTX_DATA_ACTIVITY:
                    watch_display_text(WATCH_POSITION_BOTTOM, "ACTIV ");
                    break;
                case PTX_DATA_CUSTOM:
                    watch_display_text(WATCH_POSITION_BOTTOM, "FILE  ");
                    break;
                case PTX_DATA_COUNT:
                default:
                    watch_display_text(WATCH_POSITION_BOTTOM, "----  ");
                    break;
            }
            break;

        case PTX_MODE_CONFIG:
            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "CF", "Config");

            // Show reliability level with enhanced encoding indicators
            switch (state->reliability_level) {
                case PENTA_SPEED_PRIORITY:
                    watch_display_text(WATCH_POSITION_BOTTOM, "3b 45b"); // 3-bit, ~45bps
                    break;
                case PENTA_BALANCED:
                    watch_display_text(WATCH_POSITION_BOTTOM, "2b 30b"); // 2-bit enhanced, ~30bps
                    break;
                case PENTA_RELIABILITY_PRIORITY:
                    watch_display_text(WATCH_POSITION_BOTTOM, "1b 8bp"); // 1-bit + voting, ~8bps
                    break;
                case PENTA_MUSICAL_MODE:
                    watch_display_text(WATCH_POSITION_BOTTOM, "2b MUS"); // 2-bit musical
                    break;
            }
            break;

        case PTX_MODE_COUNTDOWN:
            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "CD", "Count");

            // Show countdown number (3, 2, 1)
            uint8_t countdown_num = 3 - (state->countdown_phase / 8);
            if (countdown_num > 0) {
                char countdown_str[7];
                snprintf(countdown_str, sizeof(countdown_str), "%d     ", countdown_num);
                watch_display_text(WATCH_POSITION_BOTTOM, countdown_str);
            } else {
                watch_display_text(WATCH_POSITION_BOTTOM, "GO    ");
            }
            break;

        case PTX_MODE_TRANSMITTING:
            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "TX", "Xmit");

            if (penta_is_transmitting(&state->encoder)) {
                // Show progress
                const penta_stats_t *stats = penta_get_stats(&state->encoder);
                char progress[7];
                if (state->data_length > 0) {
                    uint8_t percent = (stats->bytes_transmitted * 100) / state->data_length;
                    if (percent > 99) percent = 99;
                    snprintf(progress, sizeof(progress), "%2d%%  ", percent);
                } else {
                    strcpy(progress, "---   ");
                }
                watch_display_text(WATCH_POSITION_BOTTOM, progress);
            } else {
                watch_display_text(WATCH_POSITION_BOTTOM, "DONE  ");
            }
            break;

        case PTX_MODE_COMPLETE:
            if (state->show_stats) {
                const penta_stats_t *stats = penta_get_stats(&state->encoder);
                char display[7];

                switch (state->stats_page) {
                    case 0: // Blocks sent
                        watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "BL", "Blocks");
                        snprintf(display, sizeof(display), "%5d ", stats->blocks_sent);
                        watch_display_text(WATCH_POSITION_BOTTOM, display);
                        break;

                    case 1: // Bytes sent
                        watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "BY", "Bytes");
                        snprintf(display, sizeof(display), "%5d ", stats->bytes_transmitted);
                        watch_display_text(WATCH_POSITION_BOTTOM, display);
                        break;

                    case 2: // Retransmissions
                        watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "RT", "Retry");
                        snprintf(display, sizeof(display), "%5d ", stats->blocks_retransmitted);
                        watch_display_text(WATCH_POSITION_BOTTOM, display);
                        break;

                    case 3: // CRC errors
                        watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "ER", "Error");
                        snprintf(display, sizeof(display), "%5d ", stats->crc_errors);
                        watch_display_text(WATCH_POSITION_BOTTOM, display);
                        break;
                }
            } else {
                watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "OK", "Done");
                watch_display_text(WATCH_POSITION_BOTTOM, "SUCCES");
            }
            break;
    }
}

// Completion callback
static void ptx_transmission_complete(bool success, const penta_stats_t *stats) {
    (void)stats;

    if (g_ptx_state) {
        g_ptx_state->mode = PTX_MODE_COMPLETE;
        if (success) {
            watch_set_indicator(WATCH_INDICATOR_SIGNAL);
        } else {
            watch_set_indicator(WATCH_INDICATOR_LAP);
        }
        watch_set_buzzer_off();
        watch_clear_indicator(WATCH_INDICATOR_BELL);
        movement_request_tick_frequency(1);
    }
}

// Countdown tick function
static void ptx_countdown_tick(pentatonic_tx_state_t *state) {
    // Countdown complete - start transmission
    if (state->countdown_phase >= 24) { // 3 seconds * 8 ticks per second
        ptx_start_transmission(state);
        return;
    }

    // Play beep every 8 ticks (once per second)
    if ((state->countdown_phase % 8) == 0) {
        watch_set_buzzer_period_and_duty_cycle(1136, 25); // A5 note
        watch_set_buzzer_on();
    } else if ((state->countdown_phase % 8) == 1) {
        watch_set_buzzer_off();
    }

    state->countdown_phase++;
    ptx_update_display(state);
}

// Start countdown
static void ptx_start_countdown(pentatonic_tx_state_t *state) {
    state->mode = PTX_MODE_COUNTDOWN;
    state->countdown_phase = 0;

    // Request high frequency ticks
    movement_request_tick_frequency(64);
    watch_set_indicator(WATCH_INDICATOR_BELL);

    ptx_update_display(state);
}

// Start transmission
static void ptx_start_transmission(pentatonic_tx_state_t *state) {
    penta_config_t config;
    movement_request_tick_frequency(64);
    watch_set_buzzer_off(); // Clear any previous buzzer state
    penta_get_next_byte_t data_callback = NULL;

    // Prepare data source
    state->data_pos = 0;

    switch (state->data_source) {
        case PTX_DATA_DEMO:
            state->data_length = strlen(demo_message);
            data_callback = ptx_get_demo_byte;
            break;

        case PTX_DATA_COUNT:
        default:
            // Invalid data source
            return;
    }

    if (!data_callback || state->data_length == 0) {
        // No data to send
        return;
    }

    // Get configuration for reliability level
    penta_get_default_config(state->reliability_level, &config);

    // Initialize encoder
    if (penta_init_encoder_with_config(&state->encoder, &config, data_callback, ptx_transmission_complete) == PENTA_SUCCESS) {
        state->mode = PTX_MODE_TRANSMITTING;
        state->tick_count = 0;
        state->tick_divisor = 3;

        // Request high frequency ticks and show transmission indicator
        watch_set_indicator(WATCH_INDICATOR_BELL);
    }
}

bool pentatonic_tx_face_loop(movement_event_t event, void *context) {
    pentatonic_tx_state_t *state = (pentatonic_tx_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            ptx_update_display(state);
            break;

        case EVENT_MODE_BUTTON_UP:
            if (state->mode == PTX_MODE_TRANSMITTING || state->mode == PTX_MODE_COUNTDOWN) {
                // Don't exit while transmitting or counting down
                break;
            }
            movement_move_to_next_face();
            break;

        case EVENT_ALARM_BUTTON_UP:
            if (state->mode == PTX_MODE_SELECT) {
                // Go to config mode
                state->mode = PTX_MODE_CONFIG;
                ptx_update_display(state);
            } else if (state->mode == PTX_MODE_CONFIG) {
                // Start countdown
                ptx_start_countdown(state);
            } else if (state->mode == PTX_MODE_COUNTDOWN) {
                // Abort countdown
                state->mode = PTX_MODE_SELECT;
                watch_set_buzzer_off();
                watch_clear_indicator(WATCH_INDICATOR_BELL);
                movement_request_tick_frequency(1);
                ptx_update_display(state);
            } else if (state->mode == PTX_MODE_TRANSMITTING) {
                // Abort transmission
                penta_abort_transmission(&state->encoder);
                watch_set_buzzer_off();
                watch_clear_indicator(WATCH_INDICATOR_BELL);
                movement_request_tick_frequency(1);
                state->mode = PTX_MODE_SELECT;
                ptx_update_display(state);
            } else if (state->mode == PTX_MODE_COMPLETE) {
                if (state->show_stats) {
                    // Cycle through stats pages
                    state->stats_page = (state->stats_page + 1) % 4;
                    ptx_update_display(state);
                } else {
                    // Return to select mode
                    watch_clear_indicator(WATCH_INDICATOR_SIGNAL);
                    watch_clear_indicator(WATCH_INDICATOR_LAP);
                    state->mode = PTX_MODE_SELECT;
                    ptx_update_display(state);
                }
            }
            break;

        case EVENT_ALARM_LONG_PRESS:
            if (state->mode == PTX_MODE_SELECT) {
                // Start countdown directly with current settings
                ptx_start_countdown(state);
            }
            break;

        case EVENT_TICK:
            if (state->mode == PTX_MODE_COUNTDOWN) {
                state->tick_count++;
                if (state->tick_count >= 8) { // 8 ticks = ~125ms at 64Hz
                    state->tick_count = 0;
                    ptx_countdown_tick(state);
                }
            } else if (state->mode == PTX_MODE_TRANSMITTING && penta_is_transmitting(&state->encoder)) {
                state->tick_count++;
                if (state->tick_count >= state->tick_divisor) {
                    state->tick_count = 0;

                    // Get next tone
                    uint8_t tone = penta_get_next_tone(&state->encoder);
                    //printf("Next tone: %d\n", tone);
                    if (tone != 255) {
                        uint16_t period = penta_get_tone_period(tone);
                        if (period > 0) {
                            watch_set_buzzer_period_and_duty_cycle(period, 25);
                            watch_set_buzzer_on();
                        } else {
                            watch_set_buzzer_off(); // Silence
                        }
                        state->current_tone = tone;
                    }

                    // Update display periodically
                    if ((state->tick_count % 16) == 0) {
                        ptx_update_display(state);
                    }
                }
            }
            break;

        case EVENT_TIMEOUT:
            if (state->mode != PTX_MODE_TRANSMITTING && state->mode != PTX_MODE_COUNTDOWN) {
                movement_move_to_face(0);
            }
            break;

        default:
            break;
    }

    // Don't sleep while transmitting or counting down
    return (state->mode != PTX_MODE_TRANSMITTING && state->mode != PTX_MODE_COUNTDOWN);
}

void pentatonic_tx_face_resign(void *context) {
    pentatonic_tx_state_t *state = (pentatonic_tx_state_t *)context;

    // Clean up
    if (state->data_buffer) {
        free(state->data_buffer);
        state->data_buffer = NULL;
    }

    watch_set_buzzer_off();
    watch_clear_indicator(WATCH_INDICATOR_BELL);
    watch_clear_indicator(WATCH_INDICATOR_SIGNAL);
    watch_clear_indicator(WATCH_INDICATOR_LAP);

    g_ptx_state = NULL;
}

// Data source callback implementations
static uint8_t ptx_get_demo_byte(uint8_t *next_byte) {
    if (!g_ptx_state || g_ptx_state->data_pos >= strlen(demo_message)) {
        return 0; // End of data
    }

    *next_byte = demo_message[g_ptx_state->data_pos];
    g_ptx_state->data_pos++;
    return 1;
}

static uint8_t ptx_get_time_byte(uint8_t *next_byte) {
    if (!g_ptx_state || g_ptx_state->data_pos >= 8) {
        return 0; // End of data
    }

    // Get current time as Unix timestamp
    static uint32_t timestamp = 0;
    static uint32_t timezone = 0;

    if (g_ptx_state->data_pos == 0) {
        watch_date_time_t dt = watch_rtc_get_date_time();
        // Convert to Unix timestamp (simplified)
        timestamp = (dt.unit.year + 2020 - 1970) * 365 * 24 * 3600 +
                   dt.unit.month * 30 * 24 * 3600 +
                   dt.unit.day * 24 * 3600 +
                   dt.unit.hour * 3600 +
                   dt.unit.minute * 60 +
                   dt.unit.second;
        timezone = 0; // UTC for simplicity
    }

    if (g_ptx_state->data_pos < 4) {
        *next_byte = (timestamp >> (8 * (3 - g_ptx_state->data_pos))) & 0xFF;
    } else {
        *next_byte = (timezone >> (8 * (7 - g_ptx_state->data_pos))) & 0xFF;
    }

    g_ptx_state->data_pos++;
    return 1;
}

static uint8_t ptx_get_activity_byte(uint8_t *next_byte) {
    if (!g_ptx_state || !g_ptx_state->data_buffer ||
        g_ptx_state->data_pos >= g_ptx_state->data_length) {
        return 0; // End of data
    }

    *next_byte = g_ptx_state->data_buffer[g_ptx_state->data_pos];
    g_ptx_state->data_pos++;
    return 1;
}

static uint8_t ptx_get_custom_byte(uint8_t *next_byte) {
    return ptx_get_activity_byte(next_byte); // Same implementation
}
