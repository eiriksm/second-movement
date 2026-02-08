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

#include "light_binary_protocol.h"

// Start marker: 1,1,0,0 — breaks the alternating sync pattern uniquely.
static const uint8_t start_marker[LBP_START_BITS] = {1, 1, 0, 0};

// ============================================================================
// Threshold
// ============================================================================

void lbp_threshold_init(lbp_threshold_t *thr) {
    thr->cal_min = 65535;
    thr->cal_max = 0;
    thr->cal_samples = 0;
    thr->threshold = 32768;
    thr->hysteresis = 25;
    thr->current_bit = 0;
    thr->calibrated = false;
}

bool lbp_threshold_feed(lbp_threshold_t *thr, uint16_t adc_val) {
    if (adc_val < thr->cal_min) thr->cal_min = adc_val;
    if (adc_val > thr->cal_max) thr->cal_max = adc_val;
    thr->cal_samples++;

    uint16_t range = thr->cal_max - thr->cal_min;
    thr->threshold = thr->cal_min + range / 2;
    if (range >= LBP_MIN_RANGE) {
        thr->hysteresis = range / LBP_HYSTERESIS_DIV;
    }

    if (!thr->calibrated && thr->cal_samples >= LBP_CAL_SAMPLE_TARGET) {
        thr->calibrated = true;
        return true;
    }
    return false;
}

uint8_t lbp_threshold_decode(lbp_threshold_t *thr, uint16_t adc_val) {
    if (adc_val > thr->threshold + thr->hysteresis) {
        thr->current_bit = 1;
    } else if (adc_val < thr->threshold - thr->hysteresis) {
        thr->current_bit = 0;
    }
    return thr->current_bit;
}

void lbp_threshold_recalibrate(lbp_threshold_t *thr) {
    thr->cal_min = 65535;
    thr->cal_max = 0;
    thr->cal_samples = 0;
    thr->calibrated = false;
}

// ============================================================================
// Decoder
// ============================================================================

void lbp_decoder_init(lbp_decoder_t *dec) {
    dec->state = LBP_STATE_SYNC;
    dec->sync_count = 0;
    dec->start_index = 0;
    dec->bit_buf = 0;
    dec->bit_count = 0;
    dec->payload_len = 0;
    dec->payload_index = 0;
    dec->crc_accum = 0;
}

void lbp_decoder_reset(lbp_decoder_t *dec) {
    lbp_decoder_init(dec);
}

static bool push_bit(lbp_decoder_t *dec, uint8_t bit) {
    dec->bit_buf = (dec->bit_buf << 1) | (bit & 1);
    dec->bit_count++;
    return (dec->bit_count >= 8);
}

static uint8_t pop_byte(lbp_decoder_t *dec) {
    uint8_t byte = dec->bit_buf;
    dec->bit_buf = 0;
    dec->bit_count = 0;
    return byte;
}

lbp_decode_state_t lbp_decoder_push_bit(lbp_decoder_t *dec, uint8_t bit) {
    switch (dec->state) {
        case LBP_STATE_SYNC: {
            uint8_t expected = dec->sync_count & 1;
            if (bit == expected) {
                dec->sync_count++;
                if (dec->sync_count >= LBP_SYNC_BITS) {
                    dec->state = LBP_STATE_START;
                    dec->start_index = 0;
                }
            } else if (bit == 0) {
                dec->sync_count = 1;
            } else {
                dec->sync_count = 0;
            }
            break;
        }

        case LBP_STATE_START: {
            if (bit == start_marker[dec->start_index]) {
                dec->start_index++;
                if (dec->start_index >= LBP_START_BITS) {
                    dec->state = LBP_STATE_LENGTH;
                    dec->bit_buf = 0;
                    dec->bit_count = 0;
                }
            } else {
                dec->state = LBP_STATE_SYNC;
                dec->sync_count = 0;
                dec->start_index = 0;
            }
            break;
        }

        case LBP_STATE_LENGTH: {
            if (push_bit(dec, bit)) {
                uint8_t len = pop_byte(dec);
                if (len == 0 || len > LBP_MAX_PAYLOAD) {
                    dec->state = LBP_STATE_ERROR;
                    break;
                }
                dec->payload_len = len;
                dec->payload_index = 0;
                dec->crc_accum = len;
                dec->state = LBP_STATE_DATA;
            }
            break;
        }

        case LBP_STATE_DATA: {
            if (push_bit(dec, bit)) {
                uint8_t byte = pop_byte(dec);
                dec->payload[dec->payload_index++] = byte;
                dec->crc_accum ^= byte;
                if (dec->payload_index >= dec->payload_len) {
                    dec->state = LBP_STATE_CRC;
                }
            }
            break;
        }

        case LBP_STATE_CRC: {
            if (push_bit(dec, bit)) {
                uint8_t received_crc = pop_byte(dec);
                if (received_crc == dec->crc_accum) {
                    dec->state = LBP_STATE_DONE;
                } else {
                    dec->state = LBP_STATE_ERROR;
                }
            }
            break;
        }

        case LBP_STATE_DONE:
        case LBP_STATE_ERROR:
            break;
    }

    return dec->state;
}

// ============================================================================
// Encoder
// ============================================================================

void lbp_encoder_init(lbp_encoder_t *enc, const uint8_t *payload, uint8_t len) {
    enc->payload = payload;
    enc->payload_len = len;
    enc->bit_index = 0;
    enc->total_bits = lbp_frame_bits(len);

    // Pre-compute CRC: XOR of length byte and all payload bytes
    enc->crc = len;
    for (uint8_t i = 0; i < len; i++) {
        enc->crc ^= payload[i];
    }
}

bool lbp_encoder_next_bit(lbp_encoder_t *enc, uint8_t *out_bit) {
    if (enc->bit_index >= enc->total_bits) return false;

    uint16_t i = enc->bit_index++;

    // SYNC: alternating 0,1,0,1,...
    if (i < LBP_SYNC_BITS) {
        *out_bit = i & 1;
        return true;
    }
    i -= LBP_SYNC_BITS;

    // START: 1,1,0,0
    if (i < LBP_START_BITS) {
        *out_bit = start_marker[i];
        return true;
    }
    i -= LBP_START_BITS;

    // LEN: 8 bits, MSB first
    if (i < LBP_LEN_BITS) {
        *out_bit = (enc->payload_len >> (7 - i)) & 1;
        return true;
    }
    i -= LBP_LEN_BITS;

    // DATA: N*8 bits, MSB first per byte
    uint16_t data_bits = (uint16_t)enc->payload_len * 8;
    if (i < data_bits) {
        uint16_t byte_idx = i / 8;
        uint8_t bit_pos = 7 - (i % 8);
        *out_bit = (enc->payload[byte_idx] >> bit_pos) & 1;
        return true;
    }
    i -= data_bits;

    // CRC: 8 bits, MSB first
    if (i < LBP_CRC_BITS) {
        *out_bit = (enc->crc >> (7 - i)) & 1;
        return true;
    }

    return false;
}
