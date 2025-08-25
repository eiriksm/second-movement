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
#include "pentatonic_tx.h"

// Original pentatonic scale frequencies (tight spacing)
static const uint16_t penta_frequencies_original[10] = {
    440,  // A4  - Tone 0
    495,  // B4  - Tone 1
    554,  // C#5 - Tone 2
    660,  // E5  - Tone 3
    740,  // F#5 - Tone 4
    880,  // A5  - Tone 5
    990,  // B5  - Tone 6
    1108, // C#6 - Tone 7
    1320, // E6  - Tone 8 (Control)
    0     // Silence - Tone 9
};

// Enhanced frequencies with wide spacing (recommended for reliability)
static const uint16_t penta_frequencies_enhanced[10] = {
    330,  // E4  - Tone 0 (2-bit: 00) [lower baseline for better separation]
    550,  // C#5 - Tone 1 (2-bit: 01) [+220Hz gap]
    880,  // A5  - Tone 2 (2-bit: 10) [+330Hz gap]
    1320, // E6  - Tone 3 (2-bit: 11) [+440Hz gap]
    330,  // E4  - Tone 4 (voting: repeat for reliability)
    880,  // A5  - Tone 5 (voting: octave+fifth for 1-bit)
    1320, // E6  - Tone 6 (reserved)
    1760, // A6  - Tone 7 (reserved) 
    2200, // C#7 - Tone 8 (Control - very high for clear distinction)
    0     // Silence - Tone 9
};

// Removed global frequency table - now stored per encoder instance

// Removed global periods - now computed per encoder instance

// Musical start sequence for original frequencies
static const uint8_t musical_start_sequence_original[] = {0, 2, 4, 7, 9, 9}; // A-C#-F#-C#6, silence, silence
static const uint8_t musical_end_sequence_original[] = {7, 4, 2, 0, 9, 9};   // C#6-F#-C#-A, silence, silence

// Synchronization patterns for timing recovery
static const uint8_t sync_pattern_long[] = {8, 9, 8, 9, 8, 9, 8}; // Control-silence alternating
static const uint8_t sync_pattern_short[] = {8, 9, 8}; // Brief sync

// AUTOMATIC CALIBRATION SEQUENCE FOR RX CLOCK DETECTION
// This sequence is automatically sent at the start of every transmission
// to allow the RX web app to detect hardware vs simulator clock differences.
//
// The sequence provides two reference frequencies (A4 and A5 tones)
// that the RX can measure to calculate the actual hardware clock rate.
//
// WEB APP RX IMPLEMENTATION:
// 1. Detect this calibration sequence at transmission start
// 2. Measure actual frequencies of CALIBRATION_A4 and CALIBRATION_A5 tones  
// 3. Calculate frequency multiplier: measured_freq / expected_freq
// 4. Apply multiplier to all subsequent tone detection
// 5. This handles simulator (exact 1MHz) vs real hardware (e.g. 1.003MHz) differences

#define CALIBRATION_TONE_DURATION 8    // Extended duration for accurate frequency measurement
#define CALIBRATION_SILENCE_DURATION 3 // Brief silence between calibration tones

static const uint8_t calibration_sequence[] = {
    // Phase 0-7: Long A4 tone (440Hz base) - RX measures this frequency
    PENTA_CALIBRATION_TONE_A4, PENTA_CALIBRATION_TONE_A4, PENTA_CALIBRATION_TONE_A4, PENTA_CALIBRATION_TONE_A4,
    PENTA_CALIBRATION_TONE_A4, PENTA_CALIBRATION_TONE_A4, PENTA_CALIBRATION_TONE_A4, PENTA_CALIBRATION_TONE_A4,
    
    // Phase 8-10: Silence gap
    PENTA_SILENCE_TONE, PENTA_SILENCE_TONE, PENTA_SILENCE_TONE,
    
    // Phase 11-18: Long A5 tone (880Hz base, perfect octave) - RX measures this for ratio validation  
    PENTA_CALIBRATION_TONE_A5, PENTA_CALIBRATION_TONE_A5, PENTA_CALIBRATION_TONE_A5, PENTA_CALIBRATION_TONE_A5,
    PENTA_CALIBRATION_TONE_A5, PENTA_CALIBRATION_TONE_A5, PENTA_CALIBRATION_TONE_A5, PENTA_CALIBRATION_TONE_A5,
    
    // Phase 19-21: Silence gap  
    PENTA_SILENCE_TONE, PENTA_SILENCE_TONE, PENTA_SILENCE_TONE,
    
    // Phase 22: Control tone marks end of calibration, start of data transmission
    PENTA_CONTROL_TONE
};

#define CALIBRATION_SEQUENCE_LENGTH (sizeof(calibration_sequence))

// Musical start sequence for enhanced frequencies (2-bit encoding) 
static const uint8_t musical_start_sequence_enhanced[] = {0, 1, 2, 3, 9, 9}; // E4-C#5-A5-E6, silence, silence
static const uint8_t musical_end_sequence_enhanced[] = {3, 2, 1, 0, 9, 9};   // E6-A5-C#5-E4, silence, silence

// Initialize periods for specific encoder
static void _penta_init_periods(penta_encoder_state_t *encoder) {
    const uint16_t *frequencies = encoder->config.use_enhanced_encoding ? 
                                  penta_frequencies_enhanced : penta_frequencies_original;
    
    for (int i = 0; i < 9; i++) {
        encoder->tone_periods[i] = 1000000 / frequencies[i];
    }
    encoder->tone_periods[9] = 0; // Silence
}

// Update periods when frequency table changes
static void _penta_update_periods(penta_encoder_state_t *encoder) {
    const uint16_t *frequencies = encoder->config.use_enhanced_encoding ? 
                                  penta_frequencies_enhanced : penta_frequencies_original;
    
    for (int i = 0; i < 9; i++) {
        encoder->tone_periods[i] = 1000000 / frequencies[i];
    }
    encoder->tone_periods[9] = 0; // Silence
}

// CRC-8 implementation (same polynomial as used in chirpy_tx for compatibility)
uint8_t penta_crc8(const uint8_t *data, uint16_t length) {
    uint8_t crc = 0;
    for (uint16_t i = 0; i < length; i++) {
        uint8_t byte_val = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ byte_val) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C;
            }
            byte_val >>= 1;
        }
    }
    return crc;
}

// Get default configuration based on reliability level
void penta_get_default_config(penta_reliability_level_t level, penta_config_t *config) {
    if (!config) return;

    // Common defaults
    config->reliability_level = level;
    config->enable_musical_framing = true;
    config->enable_adaptive_timing = false;
    config->use_enhanced_encoding = false;
    config->enable_triple_voting = false;

    switch (level) {
        case PENTA_SPEED_PRIORITY:
            config->block_size = 32;
            config->block_repetitions = 1; // No redundancy for speed
            config->tone_duration_ms = 25;
            config->silence_duration_ms = 8;
            config->use_enhanced_encoding = false; // Use original 3-bit encoding
            break;

        case PENTA_BALANCED:
            config->block_size = 16;
            config->block_repetitions = 2; // Send each block twice
            config->tone_duration_ms = 40;
            config->silence_duration_ms = 15;
            config->use_enhanced_encoding = true; // Use 2-bit wide spacing
            break;

        case PENTA_RELIABILITY_PRIORITY:
            config->block_size = 8;
            config->block_repetitions = 3; // Triple redundancy
            config->tone_duration_ms = 60;
            config->silence_duration_ms = 25;
            config->enable_adaptive_timing = true;
            config->use_enhanced_encoding = true;
            config->enable_triple_voting = true; // Maximum reliability
            break;

        case PENTA_MUSICAL_MODE:
            config->block_size = 12;
            config->block_repetitions = 2; // Moderate redundancy for musicality
            config->tone_duration_ms = 45;
            config->silence_duration_ms = 18;
            config->enable_musical_framing = true;
            config->use_enhanced_encoding = true; // Better sound quality
            break;
    }
}

// Validate configuration
penta_result_t penta_validate_config(const penta_config_t *config) {
    if (!config) return PENTA_ERROR_INVALID_PARAM;

    if (config->block_size < 4 || config->block_size > PENTA_MAX_BLOCK_SIZE) {
        return PENTA_ERROR_INVALID_PARAM;
    }

    if (config->block_repetitions == 0 || config->block_repetitions > 5) {
        return PENTA_ERROR_INVALID_PARAM;
    }

    if (config->tone_duration_ms < 10 || config->tone_duration_ms > 500) {
        return PENTA_ERROR_INVALID_PARAM;
    }

    if (config->silence_duration_ms > 100) {
        return PENTA_ERROR_INVALID_PARAM;
    }

    return PENTA_SUCCESS;
}

// Add tone to output buffer with proper error handling
static penta_result_t _penta_append_tone(penta_encoder_state_t *encoder, uint8_t tone) {
    if (encoder->tone_buf_len >= sizeof(encoder->tone_buffer)) {
        encoder->stats.buffer_overflows++;
        return PENTA_ERROR_BUFFER_FULL;
    }

    encoder->tone_buffer[encoder->tone_buf_len] = tone;
    encoder->tone_buf_len++;
    return PENTA_SUCCESS;
}

// Add musical framing if enabled
static void _penta_add_start_sequence(penta_encoder_state_t *encoder) {
    if (!encoder->config.enable_musical_framing) return;

    const uint8_t *sequence;
    size_t sequence_len;

    if (encoder->config.use_enhanced_encoding) {
        sequence = musical_start_sequence_enhanced;
        sequence_len = sizeof(musical_start_sequence_enhanced);
    } else {
        sequence = musical_start_sequence_original;
        sequence_len = sizeof(musical_start_sequence_original);
    }

    for (int i = 0; i < sequence_len; i++) {
        _penta_append_tone(encoder, sequence[i]);
    }
}

static void _penta_add_end_sequence(penta_encoder_state_t *encoder) {
    if (!encoder->config.enable_musical_framing) return;

    const uint8_t *sequence;
    size_t sequence_len;

    if (encoder->config.use_enhanced_encoding) {
        sequence = musical_end_sequence_enhanced;
        sequence_len = sizeof(musical_end_sequence_enhanced);
    } else {
        sequence = musical_end_sequence_original;
        sequence_len = sizeof(musical_end_sequence_original);
    }

    for (int i = 0; i < sequence_len; i++) {
        _penta_append_tone(encoder, sequence[i]);
    }
}

// Encode bits into tones (variable bits per tone based on configuration)
static void _penta_encode_bits(penta_encoder_state_t *encoder, bool force_flush) {
    uint8_t bits_per_tone = encoder->config.use_enhanced_encoding ? 2 : 3;

    while (encoder->bits_in_accumulator >= bits_per_tone ||
           (force_flush && encoder->bits_in_accumulator > 0)) {

        uint8_t tone;
        if (encoder->bits_in_accumulator >= bits_per_tone) {
            // Extract top bits
            uint8_t mask = (1 << bits_per_tone) - 1;
            tone = (encoder->bit_accumulator >> (encoder->bits_in_accumulator - bits_per_tone)) & mask;
            encoder->bits_in_accumulator -= bits_per_tone;
            encoder->bit_accumulator &= (1U << encoder->bits_in_accumulator) - 1;
        } else {
            // Pad remaining bits
            tone = encoder->bit_accumulator << (bits_per_tone - encoder->bits_in_accumulator);
            encoder->bits_in_accumulator = 0;
            encoder->bit_accumulator = 0;
        }

        // Apply triple voting if enabled
        if (encoder->config.enable_triple_voting) {
            _penta_append_tone(encoder, tone);
            _penta_append_tone(encoder, tone);
            _penta_append_tone(encoder, tone);
        } else {
            _penta_append_tone(encoder, tone);
        }
    }
}

// Add byte to bit accumulator
static void _penta_add_byte(penta_encoder_state_t *encoder, uint8_t byte_val) {
    encoder->bit_accumulator = (encoder->bit_accumulator << 8) | byte_val;
    encoder->bits_in_accumulator += 8;
    _penta_encode_bits(encoder, false);
}

// Finish current block (add CRC and control markers)
static void _penta_finish_block(penta_encoder_state_t *encoder) {
    // Flush any remaining bits
    _penta_encode_bits(encoder, true);

    // Add sync pattern before block end for timing recovery
    if (encoder->sync_tone_counter % 4 == 0) { // Every 4th block
        for (int i = 0; i < sizeof(sync_pattern_short); i++) {
            if (_penta_append_tone(encoder, sync_pattern_short[i]) != PENTA_SUCCESS) {
                break;
            }
        }
    }
    encoder->sync_tone_counter++;
    
    // Add control tone to mark end of data
    if (_penta_append_tone(encoder, PENTA_CONTROL_TONE) != PENTA_SUCCESS) {
        return;
    }

    // Add CRC byte
    _penta_add_byte(encoder, encoder->block_crc);
    _penta_encode_bits(encoder, true);

    // Add Reed-Solomon parity if configured
    if (encoder->config.reliability_level == PENTA_RELIABILITY_PRIORITY && encoder->block_len > 0) {
        rs_encode(encoder->current_block, encoder->block_len, 
                  encoder->reed_solomon_parity, 4);
        encoder->rs_parity_len = 4;
        encoder->stats.reed_solomon_corrections++; // Track RS usage
        
        // Add parity bytes
        for (int i = 0; i < encoder->rs_parity_len; i++) {
            _penta_add_byte(encoder, encoder->reed_solomon_parity[i]);
        }
        _penta_encode_bits(encoder, true);
    }

    // Add another control tone to mark end of block
    if (_penta_append_tone(encoder, PENTA_CONTROL_TONE) != PENTA_SUCCESS) {
        return;
    }

    // Reset for next block (but preserve block data for repetitions)
    encoder->block_pos = 0;
    // Don't reset block_len or block_crc - needed for repetitions
    encoder->current_block_num++;
}

// Prepare next block of data with repetition support
static bool _penta_prepare_next_block(penta_encoder_state_t *encoder) {
    // If we're repeating a block, don't fetch new data
    if (encoder->repetitions_remaining > 1) {
        encoder->repetitions_remaining--;
        
        // Re-encode the same block
        for (int i = 0; i < encoder->block_len; i++) {
            _penta_add_byte(encoder, encoder->current_block[i]);
        }
        _penta_finish_block(encoder);
        encoder->stats.blocks_retransmitted++;
        return true;
    }
    
    // Reset for new block
    encoder->block_len = 0;
    encoder->block_crc = 0;
    
    // Try to fill a new block
    while (encoder->block_len < encoder->config.block_size) {
        uint8_t next_byte;
        if (!encoder->get_next_byte(&next_byte)) {
            // No more data
            encoder->end_of_data = true;
            break;
        }

        encoder->current_block[encoder->block_len] = next_byte;
        // Fix: Accumulate CRC properly, don't overwrite
        uint8_t temp_crc = encoder->block_crc;
        encoder->block_crc = penta_crc8((uint8_t*)&temp_crc, 1) ^ penta_crc8(&next_byte, 1);
        encoder->block_len++;
        encoder->stats.bytes_transmitted++;
    }

    if (encoder->block_len == 0) {
        return false; // No data to send
    }

    // Set up repetitions for new block
    encoder->repetitions_remaining = encoder->config.block_repetitions;
    encoder->repetitions_remaining--; // This is the first transmission

    // Encode the block
    for (int i = 0; i < encoder->block_len; i++) {
        _penta_add_byte(encoder, encoder->current_block[i]);
    }

    _penta_finish_block(encoder);
    encoder->stats.blocks_sent++;

    return true;
}

// Initialize encoder with default config
penta_result_t penta_init_encoder(penta_encoder_state_t *encoder,
                                  penta_get_next_byte_t get_next_byte,
                                  penta_completion_callback_t completion_callback) {

    if (!encoder || !get_next_byte) {
        return PENTA_ERROR_INVALID_PARAM;
    }

    // Clear state
    memset(encoder, 0, sizeof(penta_encoder_state_t));

    // Set default config
    penta_get_default_config(PENTA_BALANCED, &encoder->config);

    encoder->get_next_byte = get_next_byte;
    encoder->completion_callback = completion_callback;
    encoder->repetitions_remaining = encoder->config.block_repetitions;

    // Initialize periods for this encoder (no more globals)
    _penta_init_periods(encoder);

    // Add start sequence
    _penta_add_start_sequence(encoder);

    encoder->transmission_active = true;
    
    // Initialize calibration state - calibration will be sent automatically
    encoder->calibration_sent = false;
    encoder->calibration_phase = 0;

    return PENTA_SUCCESS;
}

// Initialize with custom config
penta_result_t penta_init_encoder_with_config(penta_encoder_state_t *encoder,
                                              const penta_config_t *config,
                                              penta_get_next_byte_t get_next_byte,
                                              penta_completion_callback_t completion_callback) {

    if (!encoder || !config || !get_next_byte) {
        return PENTA_ERROR_INVALID_PARAM;
    }

    penta_result_t result = penta_validate_config(config);
    if (result != PENTA_SUCCESS) {
        return result;
    }

    // Clear state
    memset(encoder, 0, sizeof(penta_encoder_state_t));

    // Copy config
    encoder->config = *config;
    encoder->get_next_byte = get_next_byte;
    encoder->completion_callback = completion_callback;
    encoder->repetitions_remaining = encoder->config.block_repetitions;

    // Initialize periods for this encoder (no more globals)
    _penta_init_periods(encoder);

    // Add start sequence
    _penta_add_start_sequence(encoder);

    encoder->transmission_active = true;
    
    // Initialize calibration state - calibration will be sent automatically
    encoder->calibration_sent = false;
    encoder->calibration_phase = 0;

    return PENTA_SUCCESS;
}

// Get next tone to transmit
uint8_t penta_get_next_tone(penta_encoder_state_t *encoder) {
    if (!encoder || !encoder->transmission_active) {
        return 255; // End of transmission
    }

    // AUTOMATIC CALIBRATION: Send calibration sequence first (before any other data)
    // This allows RX to detect actual hardware clock rate vs simulator rate
    if (!encoder->calibration_sent) {
        if (encoder->calibration_phase < CALIBRATION_SEQUENCE_LENGTH) {
            uint8_t calibration_tone = calibration_sequence[encoder->calibration_phase];
            encoder->calibration_phase++;
            
            // Mark calibration as complete when we finish the sequence
            if (encoder->calibration_phase >= CALIBRATION_SEQUENCE_LENGTH) {
                encoder->calibration_sent = true;
                // Continue to normal start sequence after calibration
            }
            
            return calibration_tone;
        }
    }

    // If we have tones in buffer, send those first
    if (encoder->tone_buf_pos < encoder->tone_buf_len) {
        uint8_t tone = encoder->tone_buffer[encoder->tone_buf_pos];
        encoder->tone_buf_pos++;

        // If buffer is empty, reset positions
        if (encoder->tone_buf_pos >= encoder->tone_buf_len) {
            encoder->tone_buf_pos = 0;
            encoder->tone_buf_len = 0;
        }

        return tone;
    }

    // Buffer is empty, need to prepare more data
    if (!encoder->end_of_data) {
        if (_penta_prepare_next_block(encoder)) {
            // Successfully prepared block, recursively get first tone
            return penta_get_next_tone(encoder);
        }
    }

    // No more data, finish transmission
    if (encoder->end_of_data && encoder->tone_buf_len == 0) {
        // Add long sync pattern before end for receiver timing recovery
        for (int i = 0; i < sizeof(sync_pattern_long); i++) {
            if (_penta_append_tone(encoder, sync_pattern_long[i]) != PENTA_SUCCESS) {
                break;
            }
        }
        
        _penta_add_end_sequence(encoder);
        encoder->transmission_active = false;

        // Calculate final effective bitrate
        if (encoder->stats.blocks_sent > 0) {
            // Rough estimate: bytes * 8 bits/byte, divided by total tones * tone_duration
            uint32_t total_tones = encoder->stats.blocks_sent * 20; // Rough estimate
            uint32_t total_time_ms = total_tones * encoder->config.tone_duration_ms;
            if (total_time_ms > 0) {
                encoder->stats.effective_bitrate = 
                    (encoder->stats.bytes_transmitted * 8000.0f) / total_time_ms;
            }
        }

        // Call completion callback if provided
        if (encoder->completion_callback) {
            encoder->completion_callback(true, &encoder->stats);
        }

        // Send end sequence if we just added it
        if (encoder->tone_buf_len > 0) {
            return penta_get_next_tone(encoder);
        }
    }

    return 255; // End of transmission
}

// Get tone period for buzzer - now requires encoder context
uint16_t penta_get_tone_period_for_encoder(const penta_encoder_state_t *encoder, uint8_t tone_index) {
    if (!encoder || tone_index >= 10) {
        return 0; // Silence/error
    }

    return encoder->tone_periods[tone_index];
}

// Backward compatibility wrapper (uses original frequencies)
uint16_t penta_get_tone_period(uint8_t tone_index) {
    if (tone_index >= 10) {
        return 0; // Silence
    }
    
    if (tone_index == 9) {
        return 0; // Silence
    }
    
    return 1000000 / penta_frequencies_original[tone_index];
}

// Get tone frequency for encoder
uint16_t penta_get_tone_frequency_for_encoder(const penta_encoder_state_t *encoder, uint8_t tone_index) {
    if (!encoder || tone_index >= 10) {
        return 0; // Silence/error
    }

    const uint16_t *frequencies = encoder->config.use_enhanced_encoding ? 
                                  penta_frequencies_enhanced : penta_frequencies_original;
    return frequencies[tone_index];
}

// Backward compatibility wrapper (uses original frequencies)
uint16_t penta_get_tone_frequency(uint8_t tone_index) {
    if (tone_index >= 10) {
        return 0; // Silence
    }

    return penta_frequencies_original[tone_index];
}

// Check if still transmitting
bool penta_is_transmitting(const penta_encoder_state_t *encoder) {
    return encoder && encoder->transmission_active;
}

// Abort transmission
void penta_abort_transmission(penta_encoder_state_t *encoder) {
    if (!encoder) return;

    encoder->transmission_active = false;
    encoder->tone_buf_len = 0;
    encoder->tone_buf_pos = 0;

    if (encoder->completion_callback) {
        encoder->completion_callback(false, &encoder->stats);
    }
}

// Get transmission statistics
const penta_stats_t* penta_get_stats(const penta_encoder_state_t *encoder) {
    return encoder ? &encoder->stats : NULL;
}

// Reed-Solomon functions are implemented in reed_solomon.c

// CALIBRATION HELPER FUNCTIONS FOR RX WEB APP IMPLEMENTATION
// These functions provide information about the automatic calibration sequence
// to help the RX web app detect and parse the calibration tones.

/** @brief Get the expected frequency for a calibration tone
 * @param tone_index The calibration tone index (PENTA_CALIBRATION_TONE_A4 or PENTA_CALIBRATION_TONE_A5)
 * @param use_enhanced_encoding Whether using enhanced frequency encoding
 * @return Expected frequency in Hz (what simulator transmits)
 */
uint16_t penta_get_calibration_frequency(uint8_t tone_index, bool use_enhanced_encoding) {
    const uint16_t *frequencies = use_enhanced_encoding ? 
                                  penta_frequencies_enhanced : penta_frequencies_original;
    
    if (tone_index >= 10) {
        return 0; // Invalid tone
    }
    
    return frequencies[tone_index];
}

/** @brief Get the length of the automatic calibration sequence
 * @return Number of tones in the calibration sequence
 */
uint8_t penta_get_calibration_sequence_length(void) {
    return CALIBRATION_SEQUENCE_LENGTH;
}

/** @brief Check if a transmission starts with calibration sequence
 * @param tone_buffer Buffer containing received tones
 * @param buffer_length Length of the tone buffer
 * @return 1 if buffer starts with calibration sequence, 0 otherwise
 */
int penta_detect_calibration_sequence(const uint8_t *tone_buffer, uint8_t buffer_length) {
    if (!tone_buffer || buffer_length < CALIBRATION_SEQUENCE_LENGTH) {
        return 0; // Buffer too short
    }
    
    // Check if the buffer starts with our calibration sequence
    for (int i = 0; i < CALIBRATION_SEQUENCE_LENGTH; i++) {
        if (tone_buffer[i] != calibration_sequence[i]) {
            return 0; // Doesn't match calibration sequence
        }
    }
    
    return 1; // Found calibration sequence
}

/** @brief Calculate frequency multiplier from measured calibration tones
 * @param measured_a4_hz Measured frequency of A4 calibration tone
 * @param measured_a5_hz Measured frequency of A5 calibration tone  
 * @param use_enhanced_encoding Whether transmission used enhanced encoding
 * @return Frequency multiplier (actual_clock_rate / 1MHz), or 0.0 if invalid
 */
float penta_calculate_frequency_multiplier(float measured_a4_hz, float measured_a5_hz, bool use_enhanced_encoding) {
    float expected_a4 = (float)penta_get_calibration_frequency(PENTA_CALIBRATION_TONE_A4, use_enhanced_encoding);
    float expected_a5 = (float)penta_get_calibration_frequency(PENTA_CALIBRATION_TONE_A5, use_enhanced_encoding);
    
    if (expected_a4 == 0.0f || expected_a5 == 0.0f) {
        return 0.0f; // Invalid configuration
    }
    
    // Calculate multiplier from both tones and average them for better accuracy
    float multiplier_a4 = measured_a4_hz / expected_a4;
    float multiplier_a5 = measured_a5_hz / expected_a5;
    
    // Validate that both multipliers are reasonably close (within 5%)
    float ratio = multiplier_a4 / multiplier_a5;
    if (ratio < 0.95f || ratio > 1.05f) {
        return 0.0f; // Multipliers don't match - probably measurement error
    }
    
    // Return average of both measurements
    return (multiplier_a4 + multiplier_a5) / 2.0f;
}
