/*
 * MIT License
 *
 * Copyright (c) 2026
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
#include "ir_echo_face.h"

#ifdef HAS_IR_SENSOR
#include "uart.h"
#endif

void ir_echo_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(ir_echo_state_t));
        memset(*context_ptr, 0, sizeof(ir_echo_state_t));
    }
}

void ir_echo_face_activate(void *context) {
    ir_echo_state_t *state = (ir_echo_state_t *)context;
    memset(state->received_data, 0, sizeof(state->received_data));
    state->has_data = false;

#ifdef HAS_IR_SENSOR
    // Initialize IR receiver on hardware
    HAL_GPIO_IR_ENABLE_out();
    HAL_GPIO_IR_ENABLE_clr();
    HAL_GPIO_IRSENSE_in();
    HAL_GPIO_IRSENSE_pmuxen(HAL_GPIO_PMUX_SERCOM_ALT);
    uart_init_instance(0, UART_TXPO_NONE, UART_RXPO_0, 900);
    uart_set_irda_mode_instance(0, true);
    uart_enable_instance(0);
#endif
}

bool ir_echo_face_loop(movement_event_t event, void *context) {
    ir_echo_state_t *state = (ir_echo_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
        case EVENT_NONE:
            watch_clear_display();
            watch_display_text(WATCH_POSITION_TOP, "IR    ");
            if (state->has_data) {
                watch_display_text(WATCH_POSITION_BOTTOM, state->received_data);
            } else {
                watch_display_text(WATCH_POSITION_BOTTOM, "ECHO  ");
            }
            break;

        case EVENT_TICK:
        {
#ifdef HAS_IR_SENSOR
            // Hardware mode: read from IR sensor
            char data[64];
            size_t bytes_read = uart_read_instance(0, data, 63);

            if (bytes_read > 0) {
                data[bytes_read] = '\0';

                // Trim whitespace/newlines
                while (bytes_read > 0 && (data[bytes_read-1] == '\n' || data[bytes_read-1] == '\r' || data[bytes_read-1] == ' ')) {
                    data[bytes_read-1] = '\0';
                    bytes_read--;
                }

                if (bytes_read > 0) {
                    // Calculate how much space is left in the buffer
                    size_t current_len = strlen(state->received_data);
                    size_t space_left = 6 - current_len;

                    if (space_left > 0) {
                        // Clear "ECHO" text on first byte received
                        if (current_len == 0) {
                            watch_display_text(WATCH_POSITION_BOTTOM, "      ");
                        }

                        // Append new data to existing buffer (accumulate mode)
                        size_t bytes_to_copy = (bytes_read < space_left) ? bytes_read : space_left;
                        strncat(state->received_data, data, bytes_to_copy);
                        state->received_data[6] = '\0';
                        state->has_data = true;

                        // Flash LED to indicate reception
                        movement_force_led_on(0, 48, 0);

                        // Update display
                        watch_display_text(WATCH_POSITION_BOTTOM, state->received_data);
                    } else {
                        // Buffer is full - flash yellow LED to indicate
                        movement_force_led_on(48, 48, 0);
                    }
                }
            } else {
                movement_force_led_off();
                // Blink indicator to show we're waiting
                if (watch_rtc_get_date_time().unit.second % 2 == 0) {
                    watch_set_indicator(WATCH_INDICATOR_SIGNAL);
                } else {
                    watch_clear_indicator(WATCH_INDICATOR_SIGNAL);
                }
            }
#else
            // Simulator mode: blink to show we're active
            if (watch_rtc_get_date_time().unit.second % 2 == 0) {
                watch_set_indicator(WATCH_INDICATOR_BELL);
            } else {
                watch_clear_indicator(WATCH_INDICATOR_BELL);
            }
#endif
        }
            break;

        case EVENT_ALARM_BUTTON_UP:
            // Clear the received data
            memset(state->received_data, 0, sizeof(state->received_data));
            state->has_data = false;
            watch_display_text(WATCH_POSITION_BOTTOM, "ECHO  ");
            break;

        case EVENT_TIMEOUT:
            movement_move_to_face(0);
            break;

        case EVENT_LOW_ENERGY_UPDATE:
            watch_display_text(WATCH_POSITION_TOP_RIGHT, " <");
            break;

        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void ir_echo_face_resign(void *context) {
    (void) context;
#ifdef HAS_IR_SENSOR
    uart_disable_instance(0);
    HAL_GPIO_IRSENSE_pmuxdis();
    HAL_GPIO_IRSENSE_off();
    HAL_GPIO_IR_ENABLE_off();
#endif
}
