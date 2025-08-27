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

// LFSR scrambler functions
static void _fesk_init_lfsr(fesk_encoder_state_t *encoder) {
    encoder->lfsr_state = FESK_LFSR_SEED;
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

// Base-3 packing: convert bytes to trits
static void _fesk_pack_byte_to_trits(fesk_encoder_state_t *encoder, uint8_t byte_val) {
    // Add byte to accumulator (base-256 to base-3 conversion)
    encoder->byte_accumulator = (encoder->byte_accumulator << 8) | byte_val;
    encoder->bytes_in_accumulator++;
    
    // Convert accumulated bytes to trits using radix conversion
    while (encoder->bytes_in_accumulator > 0 || encoder->byte_accumulator > 0) {
        uint8_t trit = encoder->byte_accumulator % 3;
        encoder->byte_accumulator /= 3;
        
        // Add trit to trit accumulator
        encoder->trit_accumulator = (encoder->trit_accumulator << 2) | trit; // 2 bits per trit
        encoder->trits_in_accumulator++;
        
        // If we've processed all bytes and accumulator is 0, we're done
        if (encoder->byte_accumulator == 0 && encoder->bytes_in_accumulator > 0) {
            encoder->bytes_in_accumulator--;
            if (encoder->bytes_in_accumulator == 0) break;
            // Get next byte from byte accumulator
            if (encoder->bytes_in_accumulator > 0) {
                encoder->byte_accumulator = encoder->byte_accumulator; // This is already correct
            }
        }
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

// Pack remaining bytes and flush trits
static void _fesk_flush_trits(fesk_encoder_state_t *encoder) {
    // Convert any remaining bytes in accumulator
    while (encoder->byte_accumulator > 0) {
        uint8_t trit = encoder->byte_accumulator % 3;
        encoder->byte_accumulator /= 3;
        
        encoder->trit_accumulator = (encoder->trit_accumulator << 2) | trit;
        encoder->trits_in_accumulator++;
    }
    encoder->bytes_in_accumulator = 0;
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
    encoder->trit_count = 0;
    
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
            
            // Move to header state
            encoder->state = FESK_STATE_HEADER;
            encoder->sequence_pos = 0;
            encoder->bit_pos = 0;
            
            // Pack header bytes (payload length)
            uint8_t len_hi = (encoder->payload_len >> 8) & 0xFF;
            uint8_t len_lo = encoder->payload_len & 0xFF;
            
            _fesk_pack_byte_to_trits(encoder, _fesk_scramble_byte(encoder, len_hi));
            _fesk_pack_byte_to_trits(encoder, _fesk_scramble_byte(encoder, len_lo));
            /* fallthrough */
        }
        
        case FESK_STATE_HEADER: {
            // Send header trits
            int trit = _fesk_get_next_trit(encoder);
            if (trit >= 0) {
                return (uint8_t)trit;
            }
            
            // Move to payload state
            encoder->state = FESK_STATE_PAYLOAD;
            encoder->payload_pos = 0;
            /* fallthrough */
        }
        
        case FESK_STATE_PAYLOAD: {
            // Check if we need to insert pilot
            if (encoder->config.insert_pilots && (encoder->trit_count % FESK_PILOT_INTERVAL) == 0 && encoder->trit_count > 0) {
                // Insert pilot sequence [0,2] 
                static uint8_t pilot_phase = 0;
                if (pilot_phase == 0) {
                    pilot_phase = 1;
                    return 0; // f0
                } else {
                    pilot_phase = 0;
                    encoder->trit_count++; // Count the pilot
                    return 2; // f2
                }
            }
            
            // Get next trit from accumulator
            int trit = _fesk_get_next_trit(encoder);
            if (trit >= 0) {
                encoder->trit_count++;
                return (uint8_t)trit;
            }
            
            // Need more data - pack next payload byte if available
            if (encoder->payload_pos < encoder->payload_len) {
                uint8_t byte_val = encoder->payload_buffer[encoder->payload_pos];
                encoder->payload_pos++;
                _fesk_pack_byte_to_trits(encoder, _fesk_scramble_byte(encoder, byte_val));
                
                // Try again
                trit = _fesk_get_next_trit(encoder);
                if (trit >= 0) {
                    encoder->trit_count++;
                    return (uint8_t)trit;
                }
            }
            
            // Flush any remaining trits
            _fesk_flush_trits(encoder);
            trit = _fesk_get_next_trit(encoder);
            if (trit >= 0) {
                encoder->trit_count++;
                return (uint8_t)trit;
            }
            
            // Move to CRC state
            encoder->state = FESK_STATE_CRC;
            
            // Pack CRC bytes
            uint8_t crc_hi = (encoder->crc16 >> 8) & 0xFF;
            uint8_t crc_lo = encoder->crc16 & 0xFF;
            _fesk_pack_byte_to_trits(encoder, crc_hi);
            _fesk_pack_byte_to_trits(encoder, crc_lo);
            /* fallthrough */
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