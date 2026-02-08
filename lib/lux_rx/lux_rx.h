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

#ifndef LUX_RX_H_
#define LUX_RX_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * lux_rx — optical data receiver.
 *
 * Receives arbitrary bytes (binary, UTF-8, anything) over a 1-bit light
 * channel. Just feed ADC samples and check the return value.
 *
 * Frame format:
 *   [SYNC x16] [START 1100] [LEN x8] [PAYLOAD N×8] [CRC8 x8]
 *
 * Usage (receiver):
 *   lux_rx_t rx;
 *   lux_rx_init(&rx);
 *   // each tick:
 *   if (lux_rx_feed(&rx, adc_val) == LUX_RX_DONE) {
 *       use(rx.payload, rx.payload_len);
 *       lux_rx_reset(&rx);
 *   }
 *
 * Usage (encoder):
 *   lux_rx_encoder_t enc;
 *   lux_rx_encode(&enc, data, len);
 *   uint8_t bit;
 *   while (lux_rx_encode_next(&enc, &bit)) { transmit(bit); }
 */

#define LUX_RX_MAX_PAYLOAD 128

typedef enum {
    LUX_RX_BUSY,    // calibrating, syncing, or receiving
    LUX_RX_DONE,    // frame received — read payload and payload_len
    LUX_RX_ERROR,   // CRC mismatch or invalid length
} lux_rx_status_t;

typedef struct {
    // --- public (read after LUX_RX_DONE) ---
    uint8_t payload[LUX_RX_MAX_PAYLOAD];
    uint8_t payload_len;

    // --- private ---
    uint16_t cal_min, cal_max, cal_samples;
    uint16_t threshold, hysteresis;
    uint8_t current_bit;
    bool calibrated;
    uint8_t state;
    uint8_t sync_count, start_index;
    uint8_t bit_buf, bit_count;
    uint16_t payload_index;
    uint8_t crc_accum;
} lux_rx_t;

/// Initialize receiver. Call once before feeding samples.
void lux_rx_init(lux_rx_t *rx);

/// Feed one ADC sample. Call once per tick.
/// Returns LUX_RX_BUSY while working, LUX_RX_DONE when a valid frame
/// arrives, or LUX_RX_ERROR on CRC/length failure.
lux_rx_status_t lux_rx_feed(lux_rx_t *rx, uint16_t adc_val);

/// Reset to listen for a new frame.
void lux_rx_reset(lux_rx_t *rx);

// --- Encoder (for building frames to transmit) ---

typedef struct {
    const uint8_t *payload;
    uint8_t payload_len;
    uint8_t crc;
    uint16_t bit_index, total_bits;
} lux_rx_encoder_t;

/// Prepare a frame for transmission.
void lux_rx_encode(lux_rx_encoder_t *enc, const uint8_t *data, uint8_t len);

/// Get next bit to transmit. Returns false when the frame is done.
bool lux_rx_encode_next(lux_rx_encoder_t *enc, uint8_t *out_bit);

#endif // LUX_RX_H_
