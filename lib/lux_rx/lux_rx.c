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

#include "lux_rx.h"
#include <string.h>

// Protocol constants (internal)
#define SYNC_BITS         16
#define START_BITS        4
#define CAL_SAMPLES       64
#define HYSTERESIS_DIV    8
#define MIN_RANGE         50

// Internal states (packed into rx->state)
enum {
    ST_CAL,
    ST_SYNC,
    ST_START,
    ST_LENGTH,
    ST_DATA,
    ST_CRC,
    ST_DONE,
    ST_ERROR,
};

static const uint8_t start_marker[START_BITS] = {1, 1, 0, 0};

// --- threshold (internal) ---

static void cal_feed(lux_rx_t *rx, uint16_t adc_val) {
    if (adc_val < rx->cal_min) rx->cal_min = adc_val;
    if (adc_val > rx->cal_max) rx->cal_max = adc_val;
    rx->cal_samples++;
    uint16_t range = rx->cal_max - rx->cal_min;
    rx->threshold = rx->cal_min + range / 2;
    if (range >= MIN_RANGE) rx->hysteresis = range / HYSTERESIS_DIV;
    if (!rx->calibrated && rx->cal_samples >= CAL_SAMPLES) rx->calibrated = true;
}

static uint8_t decode_bit(lux_rx_t *rx, uint16_t adc_val) {
    if (adc_val > rx->threshold + rx->hysteresis) rx->current_bit = 1;
    else if (adc_val < rx->threshold - rx->hysteresis) rx->current_bit = 0;
    return rx->current_bit;
}

// --- bit accumulator (internal) ---

static bool push_bit(lux_rx_t *rx, uint8_t bit) {
    rx->bit_buf = (rx->bit_buf << 1) | (bit & 1);
    rx->bit_count++;
    return (rx->bit_count >= 8);
}

static uint8_t pop_byte(lux_rx_t *rx) {
    uint8_t byte = rx->bit_buf;
    rx->bit_buf = 0;
    rx->bit_count = 0;
    return byte;
}

// --- decoder (internal) ---

static void process_bit(lux_rx_t *rx, uint8_t bit) {
    switch (rx->state) {
        case ST_SYNC: {
            uint8_t expected = rx->sync_count & 1;
            if (bit == expected) {
                rx->sync_count++;
                if (rx->sync_count >= SYNC_BITS) {
                    rx->state = ST_START;
                    rx->start_index = 0;
                }
            } else if (bit == 0) {
                rx->sync_count = 1;
            } else {
                rx->sync_count = 0;
            }
            break;
        }
        case ST_START:
            if (bit == start_marker[rx->start_index]) {
                rx->start_index++;
                if (rx->start_index >= START_BITS) {
                    rx->state = ST_LENGTH;
                    rx->bit_buf = 0;
                    rx->bit_count = 0;
                }
            } else {
                rx->state = ST_SYNC;
                rx->sync_count = 0;
                rx->start_index = 0;
            }
            break;
        case ST_LENGTH:
            if (push_bit(rx, bit)) {
                uint8_t len = pop_byte(rx);
                if (len == 0 || len > LUX_RX_MAX_PAYLOAD) {
                    rx->state = ST_ERROR;
                    break;
                }
                rx->payload_len = len;
                rx->payload_index = 0;
                rx->crc_accum = len;
                rx->state = ST_DATA;
            }
            break;
        case ST_DATA:
            if (push_bit(rx, bit)) {
                uint8_t byte = pop_byte(rx);
                rx->payload[rx->payload_index++] = byte;
                rx->crc_accum ^= byte;
                if (rx->payload_index >= rx->payload_len)
                    rx->state = ST_CRC;
            }
            break;
        case ST_CRC:
            if (push_bit(rx, bit)) {
                uint8_t crc = pop_byte(rx);
                rx->state = (crc == rx->crc_accum) ? ST_DONE : ST_ERROR;
            }
            break;
        default:
            break;
    }
}

// --- public API ---

void lux_rx_init(lux_rx_t *rx) {
    memset(rx, 0, sizeof(*rx));
    rx->cal_min = 65535;
    rx->threshold = 32768;
    rx->hysteresis = 25;
    rx->state = ST_CAL;
}

lux_rx_status_t lux_rx_feed(lux_rx_t *rx, uint16_t adc_val) {
    if (rx->state == ST_DONE) return LUX_RX_DONE;
    if (rx->state == ST_ERROR) return LUX_RX_ERROR;

    // Calibration phase
    if (rx->state == ST_CAL) {
        cal_feed(rx, adc_val);
        if (rx->calibrated) rx->state = ST_SYNC;
        return LUX_RX_BUSY;
    }

    // Live-update threshold during sync
    if (rx->state == ST_SYNC) cal_feed(rx, adc_val);

    uint8_t bit = decode_bit(rx, adc_val);
    process_bit(rx, bit);

    if (rx->state == ST_DONE) return LUX_RX_DONE;
    if (rx->state == ST_ERROR) return LUX_RX_ERROR;
    return LUX_RX_BUSY;
}

void lux_rx_reset(lux_rx_t *rx) {
    bool was_calibrated = rx->calibrated;
    uint16_t min = rx->cal_min, max = rx->cal_max;
    uint16_t thr = rx->threshold, hys = rx->hysteresis;
    uint16_t samples = rx->cal_samples;
    uint8_t cur = rx->current_bit;

    memset(rx, 0, sizeof(*rx));

    // Preserve calibration so re-sync is fast
    rx->calibrated = was_calibrated;
    rx->cal_min = min;
    rx->cal_max = max;
    rx->cal_samples = samples;
    rx->threshold = thr;
    rx->hysteresis = hys;
    rx->current_bit = cur;
    rx->state = ST_SYNC;
}

// --- encoder ---

#define ENC_SYNC_BITS  16
#define ENC_START_BITS 4
#define ENC_LEN_BITS   8
#define ENC_CRC_BITS   8

void lux_rx_encode(lux_rx_encoder_t *enc, const uint8_t *data, uint8_t len) {
    enc->payload = data;
    enc->payload_len = len;
    enc->bit_index = 0;
    enc->total_bits = ENC_SYNC_BITS + ENC_START_BITS + ENC_LEN_BITS
                    + (uint16_t)len * 8 + ENC_CRC_BITS;
    enc->crc = len;
    for (uint8_t i = 0; i < len; i++) enc->crc ^= data[i];
}

bool lux_rx_encode_next(lux_rx_encoder_t *enc, uint8_t *out_bit) {
    if (enc->bit_index >= enc->total_bits) return false;
    uint16_t i = enc->bit_index++;

    if (i < ENC_SYNC_BITS) { *out_bit = i & 1; return true; }
    i -= ENC_SYNC_BITS;

    if (i < ENC_START_BITS) { *out_bit = start_marker[i]; return true; }
    i -= ENC_START_BITS;

    if (i < ENC_LEN_BITS) { *out_bit = (enc->payload_len >> (7 - i)) & 1; return true; }
    i -= ENC_LEN_BITS;

    uint16_t data_bits = (uint16_t)enc->payload_len * 8;
    if (i < data_bits) {
        *out_bit = (enc->payload[i / 8] >> (7 - (i % 8))) & 1;
        return true;
    }
    i -= data_bits;

    if (i < ENC_CRC_BITS) { *out_bit = (enc->crc >> (7 - i)) & 1; return true; }
    return false;
}
