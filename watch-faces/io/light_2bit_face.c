/*
 * MIT License
 *
 * Copyright (c) 2025 Claude
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
#include "light_2bit_face.h"
#include "adc.h"

#ifdef HAS_IR_SENSOR

// Gray code table: ADC level index -> 2-bit symbol
// Gray code ensures adjacent levels differ by only 1 bit,
// so a threshold error only flips 1 bit instead of potentially 2.
//   Level 0 (darkest)  -> 0b00
//   Level 1            -> 0b01
//   Level 2            -> 0b11
//   Level 3 (brightest)-> 0b10
static const uint8_t gray_encode[4] = {0x00, 0x01, 0x03, 0x02};

// Inverse Gray decode: symbol -> level index (for display/debug)
static const uint8_t gray_decode[4] = {0, 1, 3, 2};

// Start marker pattern: symbols 01, 10, 01, 10
static const uint8_t start_marker[LIGHT_2BIT_START_LEN] = {0x01, 0x02, 0x01, 0x02};

// Available symbol rates (Hz) — must be powers of 2 for movement framework
static const uint8_t symbol_rates[] = {64, 128};
#define NUM_RATES (sizeof(symbol_rates) / sizeof(symbol_rates[0]))

// Number of calibration samples to collect before setting thresholds
#define CAL_SAMPLE_TARGET 128

// Read the IR photodiode ADC value
static uint16_t read_light(void) {
    return adc_get_analog_value(HAL_GPIO_IRSENSE_pin());
}

// Map an ADC reading to a 2-bit Gray-coded symbol using the calibrated thresholds
static uint8_t adc_to_symbol(light_2bit_context_t *ctx, uint16_t adc_val) {
    uint8_t level;
    if (adc_val < ctx->thresholds[0]) {
        level = 0;
    } else if (adc_val < ctx->thresholds[1]) {
        level = 1;
    } else if (adc_val < ctx->thresholds[2]) {
        level = 2;
    } else {
        level = 3;
    }
    return gray_encode[level];
}

// Accumulate a 2-bit symbol into the byte buffer.
// Returns true when a full byte (4 symbols) has been assembled.
static bool push_symbol(light_2bit_context_t *ctx, uint8_t symbol) {
    ctx->symbol_buf = (ctx->symbol_buf << 2) | (symbol & 0x03);
    ctx->symbol_count++;
    return (ctx->symbol_count >= 4);
}

// Get the assembled byte and reset the symbol accumulator
static uint8_t pop_byte(light_2bit_context_t *ctx) {
    uint8_t byte = ctx->symbol_buf;
    ctx->symbol_buf = 0;
    ctx->symbol_count = 0;
    return byte;
}

// Reset the receiver state machine to idle
static void reset_receiver(light_2bit_context_t *ctx) {
    ctx->sync_count = 0;
    ctx->start_index = 0;
    ctx->symbol_buf = 0;
    ctx->symbol_count = 0;
    ctx->payload_len = 0;
    ctx->payload_index = 0;
    ctx->crc_accum = 0;
}

// Process one decoded symbol through the receiver state machine
static void process_symbol(light_2bit_context_t *ctx, uint8_t symbol) {
    ctx->last_symbol = symbol;

    switch (ctx->state) {
        case LIGHT_2BIT_STATE_SYNC: {
            // Sync pattern is alternating 00, 11, 00, 11 ...
            uint8_t expected = (ctx->sync_count & 1) ? 0x03 : 0x00;
            if (symbol == expected) {
                ctx->sync_count++;
                if (ctx->sync_count >= LIGHT_2BIT_SYNC_COUNT) {
                    ctx->state = LIGHT_2BIT_STATE_START;
                    ctx->start_index = 0;
                }
            } else if (symbol == 0x00) {
                // Could be start of a new sync sequence
                ctx->sync_count = 1;
            } else {
                ctx->sync_count = 0;
            }
            break;
        }

        case LIGHT_2BIT_STATE_START: {
            if (symbol == start_marker[ctx->start_index]) {
                ctx->start_index++;
                if (ctx->start_index >= LIGHT_2BIT_START_LEN) {
                    // Start marker complete, now read the length byte
                    ctx->state = LIGHT_2BIT_STATE_LENGTH;
                    ctx->symbol_buf = 0;
                    ctx->symbol_count = 0;
                }
            } else {
                // Mismatch — go back to looking for sync
                ctx->state = LIGHT_2BIT_STATE_SYNC;
                ctx->sync_count = 0;
                ctx->start_index = 0;
            }
            break;
        }

        case LIGHT_2BIT_STATE_LENGTH: {
            if (push_symbol(ctx, symbol)) {
                uint8_t len = pop_byte(ctx);
                if (len == 0 || len > LIGHT_2BIT_MAX_PAYLOAD) {
                    ctx->state = LIGHT_2BIT_STATE_ERROR;
                    break;
                }
                ctx->payload_len = len;
                ctx->payload_index = 0;
                ctx->crc_accum = len; // CRC starts with the length byte
                ctx->state = LIGHT_2BIT_STATE_DATA;
            }
            break;
        }

        case LIGHT_2BIT_STATE_DATA: {
            if (push_symbol(ctx, symbol)) {
                uint8_t byte = pop_byte(ctx);
                ctx->payload[ctx->payload_index++] = byte;
                ctx->crc_accum ^= byte;
                if (ctx->payload_index >= ctx->payload_len) {
                    ctx->state = LIGHT_2BIT_STATE_CRC;
                }
            }
            break;
        }

        case LIGHT_2BIT_STATE_CRC: {
            if (push_symbol(ctx, symbol)) {
                uint8_t received_crc = pop_byte(ctx);
                if (received_crc == ctx->crc_accum) {
                    ctx->state = LIGHT_2BIT_STATE_DONE;
                } else {
                    ctx->state = LIGHT_2BIT_STATE_ERROR;
                }
            }
            break;
        }

        default:
            break;
    }
}

void light_2bit_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(light_2bit_context_t));
        memset(*context_ptr, 0, sizeof(light_2bit_context_t));
    }
}

void light_2bit_face_activate(void *context) {
    light_2bit_context_t *ctx = (light_2bit_context_t *)context;

    // Set up IR photodiode for ADC reading
    HAL_GPIO_IR_ENABLE_out();
    HAL_GPIO_IR_ENABLE_clr();
    HAL_GPIO_IRSENSE_pmuxen(HAL_GPIO_PMUX_ADC);
    adc_init();
    adc_enable();

    // Start in calibration mode
    ctx->state = LIGHT_2BIT_STATE_CALIBRATE;
    ctx->cal_min = 65535;
    ctx->cal_max = 0;
    ctx->cal_samples = 0;
    ctx->rate_index = 1; // default 128 Hz
    ctx->burst_mode = false;
    ctx->burst_count = 0;
    reset_receiver(ctx);

    // Request the selected tick frequency
    movement_request_tick_frequency(symbol_rates[ctx->rate_index]);
}

bool light_2bit_face_loop(movement_event_t event, void *context) {
    light_2bit_context_t *ctx = (light_2bit_context_t *)context;
    char buf[7];

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            watch_display_text_with_fallback(WATCH_POSITION_TOP, "Li 2b", "L2");
            watch_display_text(WATCH_POSITION_BOTTOM, "CAL   ");
            break;

        case EVENT_TICK:
        {
            uint16_t adc_val = read_light();

            switch (ctx->state) {
                case LIGHT_2BIT_STATE_CALIBRATE: {
                    // Track min/max over many samples to establish dynamic range
                    if (adc_val < ctx->cal_min) ctx->cal_min = adc_val;
                    if (adc_val > ctx->cal_max) ctx->cal_max = adc_val;
                    ctx->cal_samples++;

                    // Show progress
                    snprintf(buf, 7, "C %4d", ctx->cal_samples);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);

                    if (ctx->cal_samples >= CAL_SAMPLE_TARGET) {
                        // Compute thresholds dividing range into 4 equal zones
                        uint16_t range = ctx->cal_max - ctx->cal_min;
                        if (range < 100) {
                            // Not enough dynamic range — use wide default thresholds
                            // This means the transmitter needs to produce distinguishable levels
                            uint16_t mid = (ctx->cal_min + ctx->cal_max) / 2;
                            ctx->thresholds[0] = mid - 500;
                            ctx->thresholds[1] = mid;
                            ctx->thresholds[2] = mid + 500;
                        } else {
                            uint16_t quarter = range / 4;
                            ctx->thresholds[0] = ctx->cal_min + quarter;
                            ctx->thresholds[1] = ctx->cal_min + 2 * quarter;
                            ctx->thresholds[2] = ctx->cal_min + 3 * quarter;
                        }

                        ctx->state = LIGHT_2BIT_STATE_SYNC;
                        watch_display_text_with_fallback(WATCH_POSITION_TOP, "Li 2b", "L2");
                        watch_display_text(WATCH_POSITION_BOTTOM, "SYNC  ");
                    }
                    break;
                }

                case LIGHT_2BIT_STATE_SYNC: {
                    uint8_t symbol = adc_to_symbol(ctx, adc_val);
                    process_symbol(ctx, symbol);
                    // Show raw ADC and current symbol for debugging
                    snprintf(buf, 7, "S%d%4d", symbol, adc_val % 10000);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                }

                case LIGHT_2BIT_STATE_START: {
                    uint8_t symbol = adc_to_symbol(ctx, adc_val);
                    process_symbol(ctx, symbol);
                    snprintf(buf, 7, "St %2d ", ctx->start_index);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                }

                case LIGHT_2BIT_STATE_LENGTH: {
                    uint8_t symbol = adc_to_symbol(ctx, adc_val);
                    process_symbol(ctx, symbol);
                    watch_display_text(WATCH_POSITION_BOTTOM, "LEN   ");
                    break;
                }

                case LIGHT_2BIT_STATE_DATA: {
                    uint8_t symbol = adc_to_symbol(ctx, adc_val);
                    process_symbol(ctx, symbol);
                    snprintf(buf, 7, "d%3d%c%c",
                             ctx->payload_index,
                             ctx->payload_index > 0 ? ctx->payload[ctx->payload_index - 1] : ' ',
                             ' ');
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                }

                case LIGHT_2BIT_STATE_CRC: {
                    uint8_t symbol = adc_to_symbol(ctx, adc_val);
                    process_symbol(ctx, symbol);
                    watch_display_text(WATCH_POSITION_BOTTOM, "CRC   ");
                    break;
                }

                case LIGHT_2BIT_STATE_DONE: {
                    // Frame received! Show payload size
                    watch_display_text_with_fallback(WATCH_POSITION_TOP, "RECV ", "RC");
                    snprintf(buf, 7, "%4db ", ctx->payload_len);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    movement_force_led_on(0, 48, 0); // green = success
                    break;
                }

                case LIGHT_2BIT_STATE_ERROR: {
                    watch_display_text_with_fallback(WATCH_POSITION_TOP, "ERR  ", "ER");
                    watch_display_text(WATCH_POSITION_BOTTOM, "FAIL  ");
                    movement_force_led_on(48, 0, 0); // red = error
                    break;
                }

                case LIGHT_2BIT_STATE_IDLE: {
                    // Show current ADC reading and symbol rate
                    snprintf(buf, 7, "%3dHz ", symbol_rates[ctx->rate_index]);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                }
            }
            break;
        }

        case EVENT_ALARM_BUTTON_UP:
            if (ctx->state == LIGHT_2BIT_STATE_DONE || ctx->state == LIGHT_2BIT_STATE_ERROR) {
                // After result/error, go back to listening
                movement_force_led_off();
                reset_receiver(ctx);
                ctx->state = LIGHT_2BIT_STATE_SYNC;
                watch_display_text_with_fallback(WATCH_POSITION_TOP, "Li 2b", "L2");
                watch_display_text(WATCH_POSITION_BOTTOM, "SYNC  ");
            } else if (ctx->state == LIGHT_2BIT_STATE_IDLE) {
                // Cycle symbol rate
                ctx->rate_index = (ctx->rate_index + 1) % NUM_RATES;
                movement_request_tick_frequency(symbol_rates[ctx->rate_index]);
                snprintf(buf, 7, "%3dHz ", symbol_rates[ctx->rate_index]);
                watch_display_text(WATCH_POSITION_BOTTOM, buf);
            } else if (ctx->state == LIGHT_2BIT_STATE_SYNC ||
                       ctx->state == LIGHT_2BIT_STATE_START) {
                // Long idle in sync — allow recalibration
                ctx->state = LIGHT_2BIT_STATE_CALIBRATE;
                ctx->cal_min = 65535;
                ctx->cal_max = 0;
                ctx->cal_samples = 0;
                watch_display_text(WATCH_POSITION_BOTTOM, "CAL   ");
            }
            break;

        case EVENT_ALARM_LONG_PRESS:
            // Toggle burst mode
            ctx->burst_mode = !ctx->burst_mode;
            if (ctx->burst_mode) {
                watch_display_text_with_fallback(WATCH_POSITION_TOP, "BURST", "BU");
            } else {
                watch_display_text_with_fallback(WATCH_POSITION_TOP, "Li 2b", "L2");
            }
            break;

        case EVENT_LIGHT_BUTTON_UP:
            // Suppress LED — it would interfere with light sensing
            break;

        case EVENT_TIMEOUT:
            movement_move_to_face(0);
            break;

        default:
            return movement_default_loop_handler(event);
    }

    return false;
}

void light_2bit_face_resign(void *context) {
    (void) context;
    movement_force_led_off();
    adc_disable();
    HAL_GPIO_IRSENSE_pmuxdis();
    HAL_GPIO_IRSENSE_off();
    HAL_GPIO_IR_ENABLE_off();
}

#endif // HAS_IR_SENSOR
