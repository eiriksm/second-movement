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
#include "lux_rx_demo_face.h"
#include "adc.h"

#ifdef HAS_IR_SENSOR

static const uint8_t symbol_rates[] = {64, 128};
#define NUM_RATES (sizeof(symbol_rates) / sizeof(symbol_rates[0]))

static uint16_t read_light(void) {
    return adc_get_analog_value(HAL_GPIO_IRSENSE_pin());
}

void lux_rx_demo_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(lux_rx_demo_context_t));
        memset(*context_ptr, 0, sizeof(lux_rx_demo_context_t));
    }
}

void lux_rx_demo_face_activate(void *context) {
    lux_rx_demo_context_t *ctx = (lux_rx_demo_context_t *)context;

    HAL_GPIO_IR_ENABLE_out();
    HAL_GPIO_IR_ENABLE_clr();
    HAL_GPIO_IRSENSE_pmuxen(HAL_GPIO_PMUX_ADC);
    adc_init();
    adc_enable();

    lux_rx_init(&ctx->rx);
    ctx->rate_index = 1;

    movement_request_tick_frequency(symbol_rates[ctx->rate_index]);
}

bool lux_rx_demo_face_loop(movement_event_t event, void *context) {
    lux_rx_demo_context_t *ctx = (lux_rx_demo_context_t *)context;
    char buf[7];

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            watch_display_text_with_fallback(WATCH_POSITION_TOP, "LUX r", "Lr");
            watch_display_text(WATCH_POSITION_BOTTOM, "CAL   ");
            break;

        case EVENT_TICK:
        {
            lux_rx_status_t status = lux_rx_feed(&ctx->rx, read_light());
            ctx->last_status = status;

            switch (status) {
                case LUX_RX_DONE:
                    watch_display_text_with_fallback(WATCH_POSITION_TOP, "RECV ", "RC");
                    snprintf(buf, 7, "%4db ", ctx->rx.payload_len);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    movement_force_led_on(0, 48, 0);
                    break;
                case LUX_RX_ERROR:
                    watch_display_text_with_fallback(WATCH_POSITION_TOP, "ERR  ", "ER");
                    watch_display_text(WATCH_POSITION_BOTTOM, "FAIL  ");
                    movement_force_led_on(48, 0, 0);
                    break;
                case LUX_RX_BUSY:
                    if (!ctx->rx.calibrated) {
                        snprintf(buf, 7, "C %4d", ctx->rx.cal_samples);
                        watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    }
                    break;
            }
            break;
        }

        case EVENT_ALARM_BUTTON_UP:
        {
            if (ctx->last_status == LUX_RX_DONE || ctx->last_status == LUX_RX_ERROR) {
                movement_force_led_off();
                lux_rx_reset(&ctx->rx);
                watch_display_text_with_fallback(WATCH_POSITION_TOP, "LUX r", "Lr");
                watch_display_text(WATCH_POSITION_BOTTOM, "SYNC  ");
            } else {
                // Recalibrate from scratch
                lux_rx_init(&ctx->rx);
                watch_display_text_with_fallback(WATCH_POSITION_TOP, "LUX r", "Lr");
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

void lux_rx_demo_face_resign(void *context) {
    (void) context;
    movement_force_led_off();
    adc_disable();
    HAL_GPIO_IRSENSE_pmuxdis();
    HAL_GPIO_IRSENSE_off();
    HAL_GPIO_IR_ENABLE_off();
}

#endif // HAS_IR_SENSOR
