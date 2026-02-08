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

static const uint8_t symbol_rates[] = {64, 128};
#define NUM_RATES (sizeof(symbol_rates) / sizeof(symbol_rates[0]))

static uint16_t read_light(void) {
    return adc_get_analog_value(HAL_GPIO_IRSENSE_pin());
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

    lbp_threshold_init(&ctx->threshold);
    lbp_decoder_init(&ctx->decoder);
    ctx->rate_index = 1;

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
            lbp_threshold_t *thr = &ctx->threshold;
            lbp_decoder_t *dec = &ctx->decoder;

            // Phase 1: calibration (before we've seen enough samples)
            if (!thr->calibrated) {
                lbp_threshold_feed(thr, adc_val);
                snprintf(buf, 7, "C %4d", thr->cal_samples);
                watch_display_text(WATCH_POSITION_BOTTOM, buf);
                if (thr->calibrated) {
                    watch_display_text(WATCH_POSITION_BOTTOM, "SYNC  ");
                }
                break;
            }

            // Phase 2: decode bit and feed to state machine
            // Keep updating threshold during sync (transmitter sends both levels)
            if (dec->state == LBP_STATE_SYNC) {
                lbp_threshold_feed(thr, adc_val);
            }

            uint8_t bit = lbp_threshold_decode(thr, adc_val);
            lbp_decode_state_t state = lbp_decoder_push_bit(dec, bit);

            switch (state) {
                case LBP_STATE_SYNC:
                    snprintf(buf, 7, "Sy%2d %d", dec->sync_count, bit);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                case LBP_STATE_START:
                    snprintf(buf, 7, "St%2d %d", dec->start_index, bit);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                case LBP_STATE_LENGTH:
                    snprintf(buf, 7, "Ln %2d ", dec->bit_count);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                case LBP_STATE_DATA:
                    snprintf(buf, 7, "d%3d%2d", dec->payload_index, dec->bit_count);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;
                case LBP_STATE_CRC:
                    watch_display_text(WATCH_POSITION_BOTTOM, "CRC   ");
                    break;
                case LBP_STATE_DONE:
                    watch_display_text_with_fallback(WATCH_POSITION_TOP, "RECV ", "RC");
                    snprintf(buf, 7, "%4db ", dec->payload_len);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    movement_force_led_on(0, 48, 0);
                    break;
                case LBP_STATE_ERROR:
                    watch_display_text_with_fallback(WATCH_POSITION_TOP, "ERR  ", "ER");
                    watch_display_text(WATCH_POSITION_BOTTOM, "FAIL  ");
                    movement_force_led_on(48, 0, 0);
                    break;
            }
            break;
        }

        case EVENT_ALARM_BUTTON_UP:
        {
            lbp_decoder_t *dec = &ctx->decoder;
            if (dec->state == LBP_STATE_DONE || dec->state == LBP_STATE_ERROR) {
                movement_force_led_off();
                lbp_decoder_reset(dec);
                watch_display_text_with_fallback(WATCH_POSITION_TOP, "LI bI", "Lb");
                watch_display_text(WATCH_POSITION_BOTTOM, "SYNC  ");
            } else if (dec->state == LBP_STATE_SYNC || dec->state == LBP_STATE_START) {
                lbp_threshold_recalibrate(&ctx->threshold);
                lbp_decoder_reset(dec);
                watch_display_text(WATCH_POSITION_BOTTOM, "CAL   ");
            }
            break;
        }

        case EVENT_ALARM_LONG_PRESS:
            ctx->rate_index = (ctx->rate_index + 1) % NUM_RATES;
            movement_request_tick_frequency(symbol_rates[ctx->rate_index]);
            snprintf(buf, 7, "%3dHz ", symbol_rates[ctx->rate_index]);
            watch_display_text(WATCH_POSITION_BOTTOM, buf);
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
