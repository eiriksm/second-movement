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

// Start marker: 1,1,0,0 — breaks the alternating sync pattern uniquely
static const uint8_t start_marker[LIGHT_BIN_START_LEN] = {1, 1, 0, 0};

// Available symbol rates (Hz) — must be powers of 2 for movement framework
static const uint8_t symbol_rates[] = {64, 128};
#define NUM_RATES (sizeof(symbol_rates) / sizeof(symbol_rates[0]))

// Calibration: collect this many samples during the cal phase.
// During sync preamble the transmitter alternates black/white,
// so the receiver sees both extremes and calibrates from them.
#define CAL_SAMPLE_TARGET 64

// Hysteresis as a fraction of the dynamic range (1/8 = 12.5% each side)
#define HYSTERESIS_DIVISOR 8

static uint16_t read_light(void) {
    return adc_get_analog_value(HAL_GPIO_IRSENSE_pin());
}

// Decode an ADC reading to a single bit using threshold + hysteresis.
// Returns 0 (dark) or 1 (bright). If the reading falls within the
// hysteresis band, the previous bit value is retained.
static uint8_t adc_to_bit(light_2bit_context_t *ctx, uint16_t adc_val) {
    if (adc_val > ctx->threshold + ctx->hysteresis) {
        ctx->current_bit = 1;
    } else if (adc_val < ctx->threshold - ctx->hysteresis) {
        ctx->current_bit = 0;
    }
    // else: within hysteresis band, keep current_bit unchanged
    return ctx->current_bit;
}

// Push one bit into the byte accumulator. Returns true when 8 bits collected.
static bool push_bit(light_2bit_context_t *ctx, uint8_t bit) {
    ctx->bit_buf = (ctx->bit_buf << 1) | (bit & 1);
    ctx->bit_count++;
    return (ctx->bit_count >= 8);
}

// Pop the assembled byte and reset the bit accumulator.
static uint8_t pop_byte(light_2bit_context_t *ctx) {
    uint8_t byte = ctx->bit_buf;
    ctx->bit_buf = 0;
    ctx->bit_count = 0;
    return byte;
}

static void reset_receiver(light_2bit_context_t *ctx) {
    ctx->sync_count = 0;
    ctx->start_index = 0;
    ctx->bit_buf = 0;
    ctx->bit_count = 0;
    ctx->payload_len = 0;
    ctx->payload_index = 0;
    ctx->crc_accum = 0;
}

// Process one decoded bit through the receiver state machine.
static void process_bit(light_2bit_context_t *ctx, uint8_t bit) {
    switch (ctx->state) {
        case LIGHT_BIN_STATE_SYNC: {
            // Sync: expect alternating 0,1,0,1...
            uint8_t expected = ctx->sync_count & 1;
            if (bit == expected) {
                ctx->sync_count++;
                if (ctx->sync_count >= LIGHT_BIN_SYNC_COUNT) {
                    ctx->state = LIGHT_BIN_STATE_START;
                    ctx->start_index = 0;
                }
            } else if (bit == 0) {
                // Might be start of a new sync run
                ctx->sync_count = 1;
            } else {
                ctx->sync_count = 0;
            }
            break;
        }

        case LIGHT_BIN_STATE_START: {
            // Match the start marker 1,1,0,0
            if (bit == start_marker[ctx->start_index]) {
                ctx->start_index++;
                if (ctx->start_index >= LIGHT_BIN_START_LEN) {
                    ctx->state = LIGHT_BIN_STATE_LENGTH;
                    ctx->bit_buf = 0;
                    ctx->bit_count = 0;
                }
            } else {
                // Mismatch — back to sync
                ctx->state = LIGHT_BIN_STATE_SYNC;
                ctx->sync_count = 0;
                ctx->start_index = 0;
            }
            break;
        }

        case LIGHT_BIN_STATE_LENGTH: {
            if (push_bit(ctx, bit)) {
                uint8_t len = pop_byte(ctx);
                if (len == 0 || len > LIGHT_BIN_MAX_PAYLOAD) {
                    ctx->state = LIGHT_BIN_STATE_ERROR;
                    break;
                }
                ctx->payload_len = len;
                ctx->payload_index = 0;
                ctx->crc_accum = len;
                ctx->state = LIGHT_BIN_STATE_DATA;
            }
            break;
        }

        case LIGHT_BIN_STATE_DATA: {
            if (push_bit(ctx, bit)) {
                uint8_t byte = pop_byte(ctx);
                ctx->payload[ctx->payload_index++] = byte;
                ctx->crc_accum ^= byte;
                if (ctx->payload_index >= ctx->payload_len) {
                    ctx->state = LIGHT_BIN_STATE_CRC;
                }
            }
            break;
        }

        case LIGHT_BIN_STATE_CRC: {
            if (push_bit(ctx, bit)) {
                uint8_t received_crc = pop_byte(ctx);
                if (received_crc == ctx->crc_accum) {
                    ctx->state = LIGHT_BIN_STATE_DONE;
                } else {
                    ctx->state = LIGHT_BIN_STATE_ERROR;
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

    HAL_GPIO_IR_ENABLE_out();
    HAL_GPIO_IR_ENABLE_clr();
    HAL_GPIO_IRSENSE_pmuxen(HAL_GPIO_PMUX_ADC);
    adc_init();
    adc_enable();

    ctx->state = LIGHT_BIN_STATE_CALIBRATE;
    ctx->cal_min = 65535;
    ctx->cal_max = 0;
    ctx->cal_samples = 0;
    ctx->current_bit = 0;
    ctx->rate_index = 1; // default 128 Hz
    reset_receiver(ctx);

    movement_request_tick_frequency(symbol_rates[ctx->rate_index]);
}

bool light_2bit_face_loop(movement_event_t event, void *context) {
    light_2bit_context_t *ctx = (light_2bit_context_t *)context;
    char buf[7];

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            watch_display_text_with_fallback(WATCH_POSITION_TOP, "LI bI", "Lb");
            watch_display_text(WATCH_POSITION_BOTTOM, "CAL   ");
            break;

        case EVENT_TICK:
        {
            uint16_t adc_val = read_light();

            switch (ctx->state) {
                case LIGHT_BIN_STATE_CALIBRATE: {
                    if (adc_val < ctx->cal_min) ctx->cal_min = adc_val;
                    if (adc_val > ctx->cal_max) ctx->cal_max = adc_val;
                    ctx->cal_samples++;

                    snprintf(buf, 7, "C %4d", ctx->cal_samples);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);

                    if (ctx->cal_samples >= CAL_SAMPLE_TARGET) {
                        uint16_t range = ctx->cal_max - ctx->cal_min;
                        ctx->threshold = ctx->cal_min + range / 2;
                        if (range < 50) {
                            // Barely any range — use a wide default hysteresis
                            ctx->hysteresis = 25;
                        } else {
                            ctx->hysteresis = range / HYSTERESIS_DIVISOR;
                        }

                        ctx->state = LIGHT_BIN_STATE_SYNC;
                        watch_display_text_with_fallback(WATCH_POSITION_TOP, "LI bI", "Lb");
                        watch_display_text(WATCH_POSITION_BOTTOM, "SYNC  ");
                    }
                    break;
                }

                case LIGHT_BIN_STATE_SYNC: {
                    // Continue updating calibration during sync (transmitter
                    // sends alternating 0/1 so we see both extremes).
                    if (adc_val < ctx->cal_min) ctx->cal_min = adc_val;
                    if (adc_val > ctx->cal_max) ctx->cal_max = adc_val;
                    uint16_t range = ctx->cal_max - ctx->cal_min;
                    ctx->threshold = ctx->cal_min + range / 2;
                    if (range >= 50) {
                        ctx->hysteresis = range / HYSTERESIS_DIVISOR;
                    }

                    uint8_t bit = adc_to_bit(ctx, adc_val);
                    process_bit(ctx, bit);
                    snprintf(buf, 7, "Sy%2d %d", ctx->sync_count, bit);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                }

                case LIGHT_BIN_STATE_START: {
                    uint8_t bit = adc_to_bit(ctx, adc_val);
                    process_bit(ctx, bit);
                    snprintf(buf, 7, "St%2d %d", ctx->start_index, bit);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                }

                case LIGHT_BIN_STATE_LENGTH: {
                    uint8_t bit = adc_to_bit(ctx, adc_val);
                    process_bit(ctx, bit);
                    snprintf(buf, 7, "Ln %2d ", ctx->bit_count);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                }

                case LIGHT_BIN_STATE_DATA: {
                    uint8_t bit = adc_to_bit(ctx, adc_val);
                    process_bit(ctx, bit);
                    snprintf(buf, 7, "d%3d%2d",
                             ctx->payload_index, ctx->bit_count);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                }

                case LIGHT_BIN_STATE_CRC: {
                    uint8_t bit = adc_to_bit(ctx, adc_val);
                    process_bit(ctx, bit);
                    watch_display_text(WATCH_POSITION_BOTTOM, "CRC   ");
                    break;
                }

                case LIGHT_BIN_STATE_DONE: {
                    watch_display_text_with_fallback(WATCH_POSITION_TOP, "RECV ", "RC");
                    snprintf(buf, 7, "%4db ", ctx->payload_len);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    movement_force_led_on(0, 48, 0);
                    break;
                }

                case LIGHT_BIN_STATE_ERROR: {
                    watch_display_text_with_fallback(WATCH_POSITION_TOP, "ERR  ", "ER");
                    watch_display_text(WATCH_POSITION_BOTTOM, "FAIL  ");
                    movement_force_led_on(48, 0, 0);
                    break;
                }

                case LIGHT_BIN_STATE_IDLE: {
                    snprintf(buf, 7, "%3dHz ", symbol_rates[ctx->rate_index]);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                }
            }
            break;
        }

        case EVENT_ALARM_BUTTON_UP:
            if (ctx->state == LIGHT_BIN_STATE_DONE || ctx->state == LIGHT_BIN_STATE_ERROR) {
                movement_force_led_off();
                reset_receiver(ctx);
                ctx->state = LIGHT_BIN_STATE_SYNC;
                watch_display_text_with_fallback(WATCH_POSITION_TOP, "LI bI", "Lb");
                watch_display_text(WATCH_POSITION_BOTTOM, "SYNC  ");
            } else if (ctx->state == LIGHT_BIN_STATE_IDLE) {
                ctx->rate_index = (ctx->rate_index + 1) % NUM_RATES;
                movement_request_tick_frequency(symbol_rates[ctx->rate_index]);
                snprintf(buf, 7, "%3dHz ", symbol_rates[ctx->rate_index]);
                watch_display_text(WATCH_POSITION_BOTTOM, buf);
            } else if (ctx->state == LIGHT_BIN_STATE_SYNC ||
                       ctx->state == LIGHT_BIN_STATE_START) {
                // Recalibrate
                ctx->state = LIGHT_BIN_STATE_CALIBRATE;
                ctx->cal_min = 65535;
                ctx->cal_max = 0;
                ctx->cal_samples = 0;
                watch_display_text(WATCH_POSITION_BOTTOM, "CAL   ");
            }
            break;

        case EVENT_LIGHT_BUTTON_UP:
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
