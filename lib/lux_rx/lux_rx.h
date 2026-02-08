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
#include <stddef.h>

/*
 * lux_rx — binary light-sensing receiver protocol.
 *
 * Transfers arbitrary data (binary, UTF-8, anything) over a 1-bit optical
 * channel using two light levels (dark=0, bright=1).
 *
 * Frame format:
 *   [SYNC x16] [START 1100] [LEN x8] [PAYLOAD N×8] [CRC8 x8]
 *
 *   SYNC  : 16 alternating bits (0,1,0,1,...) for clock recovery & calibration
 *   START : 4-bit marker (1,1,0,0) — unique break from sync pattern
 *   LEN   : payload length in bytes (1-128), MSB first
 *   DATA  : payload bytes, MSB first
 *   CRC8  : XOR of LEN byte and all payload bytes, MSB first
 *
 * Three independent components:
 *   1. Threshold — adaptive ADC-to-bit conversion with hysteresis
 *   2. Decoder   — bit-by-bit receiver state machine
 *   3. Encoder   — zero-alloc bit iterator for transmitting frames
 */

// ============================================================================
// Protocol constants
// ============================================================================

#define LUX_RX_MAX_PAYLOAD       128
#define LUX_RX_SYNC_BITS         16
#define LUX_RX_START_BITS        4
#define LUX_RX_LEN_BITS          8
#define LUX_RX_CRC_BITS          8
#define LUX_RX_CAL_SAMPLE_TARGET 64
#define LUX_RX_HYSTERESIS_DIV    8   // hysteresis = range / 8 on each side
#define LUX_RX_MIN_RANGE         50  // minimum ADC range to consider calibrated

// ============================================================================
// Threshold: ADC → bit with adaptive calibration and hysteresis
// ============================================================================

typedef struct {
    uint16_t cal_min;
    uint16_t cal_max;
    uint16_t cal_samples;
    uint16_t threshold;
    uint16_t hysteresis;
    uint8_t current_bit;
    bool calibrated;
} lux_rx_threshold_t;

/// Initialize threshold state. Call once before use.
void lux_rx_threshold_init(lux_rx_threshold_t *thr);

/// Feed an ADC sample during calibration. Returns true when calibration is
/// complete (after LUX_RX_CAL_SAMPLE_TARGET samples). Once calibrated,
/// further calls update the threshold live (useful during sync preamble).
bool lux_rx_threshold_feed(lux_rx_threshold_t *thr, uint16_t adc_val);

/// Decode an ADC reading to 0 (dark) or 1 (bright) using the calibrated
/// threshold with hysteresis. If the reading falls within the dead zone,
/// the previous bit is retained.
uint8_t lux_rx_threshold_decode(lux_rx_threshold_t *thr, uint16_t adc_val);

/// Reset calibration so it can be run again.
void lux_rx_threshold_recalibrate(lux_rx_threshold_t *thr);

// ============================================================================
// Decoder: bit-driven receiver state machine
// ============================================================================

typedef enum {
    LUX_RX_STATE_SYNC,
    LUX_RX_STATE_START,
    LUX_RX_STATE_LENGTH,
    LUX_RX_STATE_DATA,
    LUX_RX_STATE_CRC,
    LUX_RX_STATE_DONE,
    LUX_RX_STATE_ERROR,
} lux_rx_decode_state_t;

typedef struct {
    lux_rx_decode_state_t state;
    uint8_t sync_count;
    uint8_t start_index;
    uint8_t bit_buf;
    uint8_t bit_count;
    uint8_t payload_len;
    uint16_t payload_index;
    uint8_t crc_accum;
    uint8_t payload[LUX_RX_MAX_PAYLOAD];
} lux_rx_decoder_t;

/// Initialize the decoder. Call once before use.
void lux_rx_decoder_init(lux_rx_decoder_t *dec);

/// Reset the decoder to start listening for a new frame (back to SYNC).
void lux_rx_decoder_reset(lux_rx_decoder_t *dec);

/// Push one decoded bit into the state machine. Returns the current state
/// after processing the bit. Check for LUX_RX_STATE_DONE (frame received)
/// or LUX_RX_STATE_ERROR (bad length or CRC mismatch).
lux_rx_decode_state_t lux_rx_decoder_push_bit(lux_rx_decoder_t *dec, uint8_t bit);

// ============================================================================
// Encoder: zero-allocation bit iterator for frame transmission
// ============================================================================

typedef struct {
    const uint8_t *payload;
    uint8_t payload_len;
    uint8_t crc;
    uint16_t bit_index;
    uint16_t total_bits;
} lux_rx_encoder_t;

/// Prepare a frame for transmission. Does not allocate — stores a pointer
/// to your payload buffer, which must remain valid until encoding is done.
void lux_rx_encoder_init(lux_rx_encoder_t *enc, const uint8_t *payload, uint8_t len);

/// Get the next bit to transmit. Returns true and writes *out_bit (0 or 1).
/// Returns false when the entire frame has been emitted.
/// Call this once per symbol period (tick).
bool lux_rx_encoder_next_bit(lux_rx_encoder_t *enc, uint8_t *out_bit);

/// Returns the total number of bits in the frame.
static inline uint16_t lux_rx_frame_bits(uint8_t payload_len) {
    return LUX_RX_SYNC_BITS + LUX_RX_START_BITS + LUX_RX_LEN_BITS
         + (uint16_t)payload_len * 8 + LUX_RX_CRC_BITS;
}

#endif // LUX_RX_H_
