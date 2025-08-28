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

#include <string.h>
#include <stdlib.h>
#include "fesk_tx.h"

// Preamble pattern (12-bit alternating 1010...)
static const uint8_t preamble_pattern[FESK_PREAMBLE_LEN] = {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0
};

// Barker-13 sync sequence (excellent autocorrelation properties)
static const uint8_t barker13_pattern[FESK_SYNC_LEN] = {
    1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1
};

// LFSR polynomial: x^9 + x^5 + 1 (as specified in the protocol)
#define FESK_LFSR_POLY 0x0211  // x^9 + x^5 + 1
#define FESK_LFSR_SEED 0x1FF   // Initial seed

// CRC-16/CCITT polynomial 0x1021
#define FESK_CRC16_POLY 0x1021

// Initialize periods for specific encoder
static void _fesk_init_periods(fesk_encoder_state_t *encoder) {
    encoder->tone_periods[0] = 1000000 / encoder->config.f0;
    encoder->tone_periods[1] = 1000000 / encoder->config.f1;
    encoder->tone_periods[2] = 1000000 / encoder->config.f2;
}

static uint8_t _fesk_scramble_byte(fesk_encoder_state_t *encoder, uint8_t byte_val) {
    if (!encoder->config.use_scrambler) {
        return byte_val;
    }

    uint8_t scrambled = 0;

    for (int i = 0; i < 8; i++) {
        // Extract bit 0 from LFSR
        uint8_t lfsr_bit = encoder->lfsr_state & 1;

        // XOR with input bit
        uint8_t input_bit = (byte_val >> i) & 1;
        scrambled |= (input_bit ^ lfsr_bit) << i;

        // Advance LFSR: feedback polynomial x^9 + x^5 + 1
        uint8_t feedback = ((encoder->lfsr_state >> 8) ^ (encoder->lfsr_state >> 4)) & 1;
        encoder->lfsr_state = ((encoder->lfsr_state << 1) | feedback) & 0x1FF;
    }

    return scrambled;
}

// Canonical bytes->trits MS-first (whole-number conversion)
static uint16_t pack_bytes_to_trits_msfirst(const uint8_t *bytes, uint16_t n, uint8_t *out) {
    // big-int in base-256, digits MS-first in 'work'
    uint8_t work[FESK_MAX_PAYLOAD_SIZE + 4];
    memcpy(work, bytes, n);
    uint16_t len = n;

    // collect LS-trits into tmp, then reverse once
    uint8_t tmp[FESK_MAX_TRITS];
    uint16_t tlen = 0;

    while (len > 0) {
        uint16_t newlen = 0;
        uint16_t carry = 0;
        for (uint16_t i = 0; i < len; i++) {
            uint16_t cur = (carry << 8) | work[i];
            uint8_t q = (uint8_t)(cur / 3);
            carry = (uint8_t)(cur % 3);
            if (newlen || q) work[newlen++] = q;
        }
        tmp[tlen++] = (uint8_t)carry;  // LS-trit
        len = newlen;
    }
    // reverse to MS-trit-first
    for (uint16_t i = 0; i < tlen; i++) out[i] = tmp[tlen - 1 - i];
    return tlen;
}

// LFSR scrambler functions
static void _fesk_init_lfsr(fesk_encoder_state_t *encoder) {
    encoder->lfsr_state = FESK_LFSR_SEED;
}

static void build_trit_stream(fesk_encoder_state_t *e) {
    // Rebuild the exact byte stream the spec defines:
    // [len_hi,len_lo] + payload (all scrambled) + [crc_hi,crc_lo] (unscrambled)
    uint8_t bytes[FESK_MAX_PAYLOAD_SIZE + 4];
    uint16_t m = 0;

    // Reset scrambler at frame start so TS and C align
    if (e->config.use_scrambler) _fesk_init_lfsr(e);

    uint8_t len_hi = (e->payload_len >> 8) & 0xFF;
    uint8_t len_lo = (e->payload_len) & 0xFF;
    bytes[m++] = _fesk_scramble_byte(e, len_hi);
    bytes[m++] = _fesk_scramble_byte(e, len_lo);

    for (uint16_t i = 0; i < e->payload_len; i++) {
        bytes[m++] = _fesk_scramble_byte(e, e->payload_buffer[i]);
    }

    // CRC over ORIGINAL payload (unscrambled), sent UNSCRAMBLED big-endian
    uint16_t crc = e->crc16;
    bytes[m++] = (uint8_t)(crc >> 8);
    bytes[m++] = (uint8_t)(crc & 0xFF);

    e->trit_len = pack_bytes_to_trits_msfirst(bytes, m, e->trit_stream);
    e->trit_pos = 0;
    e->data_trit_since_pilot = 0;
}


// CRC-16/CCITT implementation
uint16_t fesk_crc16(const uint8_t *data, uint16_t length, uint16_t init_value) {
    uint16_t crc = init_value;

    for (uint16_t i = 0; i < length; i++) {
        crc = fesk_update_crc16(crc, data[i]);
    }

    return crc;
}

uint16_t fesk_update_crc16(uint16_t crc, uint8_t byte_val) {
    crc ^= (uint16_t)byte_val << 8;

    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ FESK_CRC16_POLY;
        } else {
            crc <<= 1;
        }
    }

    return crc;
}

// Base-3 packing (MS-trit-first): append one byte to the base-256 big-int,
// then repeatedly divide the entire big-int by 3, collecting remainders.
// We push the resulting trits into the encoder's trit_accumulator *MS-first*.
static void _fesk_pack_byte_to_trits(fesk_encoder_state_t *encoder, uint8_t byte_val) {
    // Append byte to the big integer
    encoder->pack_work[encoder->pack_len++] = byte_val;

    // Do at least one division step each time to make progress.
    // We may produce 1..N trits depending on magnitude.
    uint8_t tmp_rems[16];   // local remainder scratch (enough for typical growth per byte)
    uint8_t rem_count = 0;

    // Perform division by 3 over the current big-int in place.
    // We keep non-zero quotient bytes compacted at the front (like big-int normalization).
    uint16_t new_len = 0;
    uint16_t i = 0;
    uint16_t carry = 0;

    while (i < encoder->pack_len) {
        uint16_t cur = (carry << 8) | encoder->pack_work[i];
        uint8_t q = (uint8_t)(cur / 3);
        carry = (uint8_t)(cur % 3);
        if (new_len || q) {
            encoder->pack_work[new_len++] = q;
        }
        i++;
    }

    // carry is the remainder in [0..2] for this division pass
    tmp_rems[rem_count++] = (uint8_t)carry;

    // Replace big-int length with compacted quotient length
    encoder->pack_len = new_len;

    // It’s common to squeeze a bit more progress while the leading quotient stays “large”.
    // Do a couple more divisions opportunistically, bounded to avoid long ISR stalls.
    // (You can tune MAX_EXTRA_DIVS; keeping it small keeps TX latency predictable.)
    enum { MAX_EXTRA_DIVS = 2 };
    for (int pass = 0; pass < MAX_EXTRA_DIVS && encoder->pack_len > 0; ++pass) {
        new_len = 0;
        carry = 0;
        for (i = 0; i < encoder->pack_len; ++i) {
            uint16_t cur = (carry << 8) | encoder->pack_work[i];
            uint8_t q = (uint8_t)(cur / 3);
            carry = (uint8_t)(cur % 3);
            if (new_len || q) {
                encoder->pack_work[new_len++] = q;
            }
        }
        tmp_rems[rem_count++] = (uint8_t)carry;
        encoder->pack_len = new_len;
    }

    // We produced rem_count LS-first trits in tmp_rems[]; emit them MS-first
    for (int r = rem_count - 1; r >= 0; --r) {
        uint8_t t = tmp_rems[r]; // 0..2
        encoder->trit_accumulator = (encoder->trit_accumulator << 2) | t;
        encoder->trits_in_accumulator++;
    }
}


// Get next trit from accumulator
static int _fesk_get_next_trit(fesk_encoder_state_t *encoder) {
    if (encoder->trits_in_accumulator == 0) {
        return -1; // No more trits
    }

    // Extract top trit
    encoder->trits_in_accumulator--;
    uint8_t trit = (encoder->trit_accumulator >> (encoder->trits_in_accumulator * 2)) & 3;

    return trit;
}

// Drain any remaining value in the big-int to trits (MS-first)
static void _fesk_flush_trits(fesk_encoder_state_t *encoder) {
    // Collect all remainders LS-first into a temporary buffer
    // then emit them reversed (MS-first) into trit_accumulator.
    if (encoder->pack_len == 0) return;

    // Worst-case number of trits is about ceil(pack_len * log_3(256)) ≈ pack_len * 5.05/3 ≈ 2x pack_len.
    // We'll generate in manageable chunks.
    while (encoder->pack_len > 0) {
        uint8_t tmp_rems[32];
        uint8_t rem_count = 0;

        // One division pass
        uint16_t new_len = 0;
        uint16_t carry = 0;
        for (uint16_t i = 0; i < encoder->pack_len; ++i) {
            uint16_t cur = (carry << 8) | encoder->pack_work[i];
            uint8_t q = (uint8_t)(cur / 3);
            carry = (uint8_t)(cur % 3);
            if (new_len || q) {
                encoder->pack_work[new_len++] = q;
            }
        }
        tmp_rems[rem_count++] = (uint8_t)carry;
        encoder->pack_len = new_len;

        // Emit the pass’s remainders MS-first
        for (int r = rem_count - 1; r >= 0; --r) {
            uint8_t t = tmp_rems[r];
            encoder->trit_accumulator = (encoder->trit_accumulator << 2) | t;
            encoder->trits_in_accumulator++;
        }
    }
}

// Get default configuration
void fesk_get_default_config(fesk_config_t *config) {
    if (!config) return;

    config->f0 = FESK_F0;
    config->f1 = FESK_F1;
    config->f2 = FESK_F2;
    config->symbol_ticks = FESK_SYMBOL_TICKS;
    config->use_scrambler = true;
    config->use_fec = false;
    config->insert_pilots = true;
}

// Validate configuration
fesk_result_t fesk_validate_config(const fesk_config_t *config) {
    if (!config) return FESK_ERROR_INVALID_PARAM;

    if (config->f0 == 0 || config->f1 == 0 || config->f2 == 0) {
        return FESK_ERROR_INVALID_PARAM;
    }

    if (config->symbol_ticks == 0 || config->symbol_ticks > 16) {
        return FESK_ERROR_INVALID_PARAM;
    }

    return FESK_SUCCESS;
}

// Initialize encoder with default config
fesk_result_t fesk_init_encoder(fesk_encoder_state_t *encoder,
                                const uint8_t *payload_data,
                                uint16_t payload_len) {

    fesk_config_t config;
    fesk_get_default_config(&config);

    return fesk_init_encoder_with_config(encoder, &config, payload_data, payload_len);
}

// Initialize with custom config
fesk_result_t fesk_init_encoder_with_config(fesk_encoder_state_t *encoder,
                                            const fesk_config_t *config,
                                            const uint8_t *payload_data,
                                            uint16_t payload_len) {

    if (!encoder || !config || !payload_data) {
        return FESK_ERROR_INVALID_PARAM;
    }

    if (payload_len > FESK_MAX_PAYLOAD_SIZE) {
        return FESK_ERROR_PAYLOAD_TOO_LARGE;
    }

    fesk_result_t result = fesk_validate_config(config);
    if (result != FESK_SUCCESS) {
        return result;
    }

    // Clear state
    memset(encoder, 0, sizeof(fesk_encoder_state_t));
    encoder->pack_len = 0;
    encoder->trit_accumulator = 0;
    encoder->trits_in_accumulator = 0;


    // Copy config
    encoder->config = *config;

    // Initialize periods
    _fesk_init_periods(encoder);

    // Copy payload data
    memcpy(encoder->payload_buffer, payload_data, payload_len);
    encoder->payload_len = payload_len;

    // Calculate CRC over original payload
    encoder->crc16 = fesk_crc16(payload_data, payload_len, 0xFFFF);

    // Initialize scrambler if enabled
    if (encoder->config.use_scrambler) {
        _fesk_init_lfsr(encoder);
    }

    // Start transmission
    encoder->state = FESK_STATE_PREAMBLE;
    encoder->transmission_active = true;
    encoder->sequence_pos = 0;
    encoder->bit_pos = 0;
    encoder->payload_pos = 0;
    encoder->pilot_phase = 0;
    encoder->trit_count  = 0;

    return FESK_SUCCESS;
}

// Get next tone to transmit
uint8_t fesk_get_next_tone(fesk_encoder_state_t *encoder) {
    if (!encoder || !encoder->transmission_active) {
        return 255; // End of transmission
    }

    switch (encoder->state) {
        case FESK_STATE_PREAMBLE: {
            // Send preamble using binary encoding (f0=0, f2=1)
            if (encoder->sequence_pos < FESK_PREAMBLE_LEN) {
                uint8_t bit = preamble_pattern[encoder->sequence_pos];
                encoder->sequence_pos++;
                return bit == 0 ? 0 : 2; // f0 for 0, f2 for 1
            }

            // Move to sync state
            encoder->state = FESK_STATE_SYNC;
            encoder->sequence_pos = 0;
            /* fallthrough */
        }

        case FESK_STATE_SYNC: {
            // Send sync using binary encoding (f0=0, f2=1)
            if (encoder->sequence_pos < FESK_SYNC_LEN) {
                uint8_t bit = barker13_pattern[encoder->sequence_pos];
                encoder->sequence_pos++;
                return bit == 0 ? 0 : 2; // f0 for 0, f2 for 1
            }

            encoder->state = FESK_STATE_HEADER;
            encoder->sequence_pos = 0;
            encoder->bit_pos = 0;

            // *** build full trit stream now ***
            build_trit_stream(encoder);

            // and jump straight to unified emission
            encoder->state = FESK_STATE_PAYLOAD;
        }

        case FESK_STATE_HEADER:
            // (no longer emit header separately)
            encoder->state = FESK_STATE_PAYLOAD;
            /* fallthrough */

        case FESK_STATE_PAYLOAD: {
            // Pilot insertion based on total data trits since sync:
            if (encoder->config.insert_pilots &&
                (encoder->trit_count % FESK_PILOT_INTERVAL) == 0 &&
                encoder->trit_count > 0) {
                if (encoder->pilot_phase == 0) { encoder->pilot_phase = 1; return 0; }
                else { encoder->pilot_phase = 0; encoder->trit_count++; return 2; }
            }

            if (encoder->trit_pos < encoder->trit_len) {
                uint8_t t = encoder->trit_stream[encoder->trit_pos++];
                encoder->trit_count++;
                return t;
            }

            // done
            encoder->state = FESK_STATE_COMPLETE;
            encoder->transmission_active = false;
            return 255;
        }

        case FESK_STATE_CRC: {
            // Send CRC trits
            int trit = _fesk_get_next_trit(encoder);
            if (trit >= 0) {
                return (uint8_t)trit;
            }

            // Flush any remaining CRC trits
            _fesk_flush_trits(encoder);
            trit = _fesk_get_next_trit(encoder);
            if (trit >= 0) {
                return (uint8_t)trit;
            }

            // Transmission complete
            encoder->state = FESK_STATE_COMPLETE;
            encoder->transmission_active = false;
            /* fallthrough */
        }

        case FESK_STATE_COMPLETE:
        default:
            return 255; // End of transmission
    }

    return 255; // Should not reach here
}

// Get tone period for buzzer
uint16_t fesk_get_tone_period(const fesk_encoder_state_t *encoder, uint8_t tone_index) {
    if (!encoder || tone_index >= FESK_TONE_COUNT) {
        return 0;
    }

    return encoder->tone_periods[tone_index];
}

// Get tone frequency
uint16_t fesk_get_tone_frequency(const fesk_encoder_state_t *encoder, uint8_t tone_index) {
    if (!encoder || tone_index >= FESK_TONE_COUNT) {
        return 0;
    }

    switch (tone_index) {
        case 0: return encoder->config.f0;
        case 1: return encoder->config.f1;
        case 2: return encoder->config.f2;
        default: return 0;
    }
}

// Check if still transmitting
bool fesk_is_transmitting(const fesk_encoder_state_t *encoder) {
    return encoder && encoder->transmission_active;
}

// Abort transmission
void fesk_abort_transmission(fesk_encoder_state_t *encoder) {
    if (!encoder) return;

    encoder->transmission_active = false;
    encoder->state = FESK_STATE_COMPLETE;
}
