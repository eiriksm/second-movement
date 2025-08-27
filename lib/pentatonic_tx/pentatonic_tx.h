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

#ifndef PENTATONIC_TX_H
#define PENTATONIC_TX_H

#include <stdint.h>
#include <stdbool.h>

/** @brief Pentatonic Data Transmission Library
 * @details A reliable, musical data transmission protocol using musical frequencies.
 *          Originally based on pentatonic scales, enhanced mode uses wide frequency spacing 
 *          for improved reliability while maintaining pleasant harmonic relationships.
 */

// Protocol configuration
#define PENTA_TONE_COUNT 8          // Number of data tones (0-7)
#define PENTA_CONTROL_TONE 8        // Control tone index
#define PENTA_SILENCE_TONE 9        // Silence/rest tone
#define PENTA_CALIBRATION_TONE_A4 0 // Calibration reference tone (440Hz in original, 330Hz in enhanced)
#define PENTA_CALIBRATION_TONE_A5 5 // Calibration high tone (880Hz in both original and enhanced)
#define PENTA_MAX_BLOCK_SIZE 32     // Maximum bytes per block
#define PENTA_DEFAULT_BLOCK_SIZE 16 // Default block size
#define PENTA_MAX_RETRIES 3         // Maximum retransmission attempts

// IMPORTANT FOR WEB APP RX IMPLEMENTATION:
// The library automatically sends a calibration sequence at the start of every transmission.
// This allows the RX to detect actual hardware clock rates vs simulator rates.
//
// CALIBRATION SEQUENCE (sent automatically before data):
// 1. Long calibration tone A4 (440Hz base, actual frequency depends on hardware clock)
// 2. Silence gap
// 3. Long calibration tone A5 (880Hz base, perfect octave for frequency ratio validation)
// 4. Silence gap  
// 5. Control tone (marks end of calibration, start of normal data)
//
// RX IMPLEMENTATION NOTES:
// - Measure the actual frequencies of the two calibration tones
// - Calculate frequency_multiplier = measured_freq / expected_freq
// - Apply this multiplier to ALL subsequent tone detection
// - This handles simulator vs hardware clock rate differences automatically
// - Typical hardware variations: ±0.1% to ±3%
//
// EXAMPLE:
// Expected: A4=440Hz, A5=880Hz
// Measured: A4=441.2Hz, A5=882.4Hz  
// Multiplier: 441.2/440 = 1.003 (hardware runs 0.3% fast)
// All subsequent detection: multiply expected frequencies by 1.003

/** @brief Transmission reliability levels
 */
typedef enum {
    PENTA_SPEED_PRIORITY,       // Fast transmission, 3-bit encoding (~45 bps)
    PENTA_BALANCED,             // 2-bit wide spacing, recommended (~30 bps)
    PENTA_RELIABILITY_PRIORITY, // 1-bit + triple voting (~8 bps)
    PENTA_MUSICAL_MODE         // Prioritizes pleasant sound (~25 bps)
} penta_reliability_level_t;

/** @brief Transmission result codes
 */
typedef enum {
    PENTA_SUCCESS = 0,
    PENTA_ERROR_INVALID_PARAM,
    PENTA_ERROR_BUFFER_FULL,
    PENTA_ERROR_TRANSMISSION_FAILED,
    PENTA_ERROR_CRC_MISMATCH,
    PENTA_ERROR_TIMEOUT,
    PENTA_ERROR_NO_DATA
} penta_result_t;

/** @brief Transmission statistics
 */
typedef struct {
    uint16_t blocks_sent;
    uint16_t blocks_retransmitted;
    uint16_t bytes_transmitted;
    uint16_t crc_errors;
    uint16_t timeouts;
    uint16_t buffer_overflows;
    uint16_t sync_failures;
    uint16_t reed_solomon_corrections;
    float effective_bitrate;
} penta_stats_t;

/** @brief Configuration for pentatonic transmission
 */
typedef struct {
    penta_reliability_level_t reliability_level;
    uint8_t block_size;                    // Bytes per block (4-32)
    uint8_t block_repetitions;             // Number of times to send each block (1-5)
    bool enable_musical_framing;           // Add melodic start/end sequences
    bool enable_adaptive_timing;           // Adjust timing based on errors
    bool use_enhanced_encoding;            // Use 2-bit wide spacing (recommended)
    bool enable_triple_voting;             // Triple repetition with majority vote
} penta_config_t;

/** @brief Callback function to get next byte of data
 * @param next_byte Pointer where next byte should be written
 * @return 1 if byte available, 0 if no more data
 */
typedef uint8_t (*penta_get_next_byte_t)(uint8_t *next_byte);

/** @brief Callback function called when transmission completes
 * @param success True if transmission successful, false if failed
 * @param stats Transmission statistics
 */
typedef void (*penta_completion_callback_t)(bool success, const penta_stats_t *stats);

/** @brief Internal encoder state - do not manipulate directly
 */
typedef struct {
    // Configuration
    penta_config_t config;
    penta_get_next_byte_t get_next_byte;
    penta_completion_callback_t completion_callback;

    // Block management
    uint8_t current_block[PENTA_MAX_BLOCK_SIZE + 8]; // +8 for FEC parity
    uint8_t block_size;
    uint8_t block_pos;
    uint8_t block_len;
    uint8_t current_block_num;
    uint8_t repetitions_remaining;

    // Tone generation
    uint8_t tone_buffer[64];
    uint8_t tone_buf_pos;
    uint8_t tone_buf_len;

    // Error correction
    uint8_t block_crc;
    uint8_t reed_solomon_parity[8];
    uint8_t rs_parity_len;

    // State tracking
    bool transmission_active;
    bool awaiting_ack;
    bool end_of_data;

    // Statistics
    penta_stats_t stats;

    // Bit manipulation for encoding
    uint32_t bit_accumulator;
    uint8_t bits_in_accumulator;

    // Per-encoder frequency tables and periods (no more globals)
    uint16_t tone_periods[10];
    
    // Synchronization and redundancy for one-way transmission
    uint8_t sync_tone_counter;
    bool sync_pattern_pending;
    uint8_t redundancy_counter;
    
    // Automatic calibration for simulator/hardware clock differences
    bool calibration_sent;          // Track if calibration sequence has been transmitted
    uint8_t calibration_phase;      // Current phase of calibration sequence (0-4)

} penta_encoder_state_t;

// Core API Functions

/** @brief Initialize pentatonic encoder with default configuration
 * @param encoder Pointer to encoder state structure
 * @param get_next_byte Callback function to fetch data bytes
 * @param completion_callback Optional callback when transmission finishes (can be NULL)
 * @return PENTA_SUCCESS or error code
 */
penta_result_t penta_init_encoder(penta_encoder_state_t *encoder,
                                  penta_get_next_byte_t get_next_byte,
                                  penta_completion_callback_t completion_callback);

/** @brief Initialize with custom configuration
 * @param encoder Pointer to encoder state structure
 * @param config Pointer to configuration structure
 * @param get_next_byte Callback function to fetch data bytes
 * @param completion_callback Optional callback when transmission finishes
 * @return PENTA_SUCCESS or error code
 */
penta_result_t penta_init_encoder_with_config(penta_encoder_state_t *encoder,
                                              const penta_config_t *config,
                                              penta_get_next_byte_t get_next_byte,
                                              penta_completion_callback_t completion_callback);

/** @brief Get the next tone to transmit
 * @param encoder Pointer to encoder state
 * @return Tone index (0-9), or 255 if transmission complete
 */
uint8_t penta_get_next_tone(penta_encoder_state_t *encoder);

/** @brief Get the period value for a given tone (for buzzer control)
 * @param tone_index Tone index (0-9)
 * @return Period value for 1MHz clock (period = 1,000,000 / frequency)
 */
uint16_t penta_get_tone_period(uint8_t tone_index);

/** @brief Get the period value for a given tone for specific encoder (recommended)
 * @param encoder Pointer to encoder state
 * @param tone_index Tone index (0-9)
 * @return Period value for 1MHz clock
 */
uint16_t penta_get_tone_period_for_encoder(const penta_encoder_state_t *encoder, uint8_t tone_index);

/** @brief Get the frequency for a given tone
 * @param tone_index Tone index (0-9)
 * @return Frequency in Hz
 */
uint16_t penta_get_tone_frequency(uint8_t tone_index);

/** @brief Get the frequency for a given tone for specific encoder (recommended)
 * @param encoder Pointer to encoder state
 * @param tone_index Tone index (0-9)
 * @return Frequency in Hz
 */
uint16_t penta_get_tone_frequency_for_encoder(const penta_encoder_state_t *encoder, uint8_t tone_index);

/** @brief Check if transmission is still active
 * @param encoder Pointer to encoder state
 * @return True if actively transmitting
 */
bool penta_is_transmitting(const penta_encoder_state_t *encoder);

/** @brief Abort current transmission
 * @param encoder Pointer to encoder state
 */
void penta_abort_transmission(penta_encoder_state_t *encoder);

/** @brief Get current transmission statistics
 * @param encoder Pointer to encoder state
 * @return Pointer to statistics structure
 */
const penta_stats_t* penta_get_stats(const penta_encoder_state_t *encoder);

// Configuration helpers

/** @brief Get default configuration for given reliability level
 * @param level Desired reliability level
 * @param config Pointer to config structure to populate
 */
void penta_get_default_config(penta_reliability_level_t level, penta_config_t *config);

/** @brief Validate configuration parameters
 * @param config Pointer to configuration to validate
 * @return PENTA_SUCCESS if valid, error code if invalid
 */
penta_result_t penta_validate_config(const penta_config_t *config);

// Utility functions

/** @brief Calculate CRC-8 for data block
 * @param data Pointer to data
 * @param length Number of bytes
 * @return CRC-8 value
 */
uint8_t penta_crc8(const uint8_t *data, uint16_t length);

// Reed-Solomon functions
void rs_encode(const uint8_t *data, uint8_t data_len, uint8_t *parity, uint8_t parity_len);
int8_t rs_decode(uint8_t *received, uint8_t data_len, uint8_t parity_len);
bool rs_validate_params(uint8_t data_len, uint8_t parity_len);

// CALIBRATION HELPER FUNCTIONS FOR RX WEB APP
// These functions help the RX web app detect and handle the automatic
// calibration sequence for simulator/hardware clock rate differences.

/** @brief Get expected frequency for calibration tone
 * @param tone_index Calibration tone (PENTA_CALIBRATION_TONE_A4 or PENTA_CALIBRATION_TONE_A5)
 * @param use_enhanced_encoding Whether using enhanced frequency encoding
 * @return Expected frequency in Hz
 */
uint16_t penta_get_calibration_frequency(uint8_t tone_index, bool use_enhanced_encoding);

/** @brief Get length of automatic calibration sequence
 * @return Number of tones in calibration sequence
 */
uint8_t penta_get_calibration_sequence_length(void);

/** @brief Detect calibration sequence in tone buffer
 * @param tone_buffer Buffer of received tones
 * @param buffer_length Length of tone buffer
 * @return 1 if calibration sequence detected, 0 otherwise
 */
int penta_detect_calibration_sequence(const uint8_t *tone_buffer, uint8_t buffer_length);

/** @brief Calculate frequency multiplier from measured calibration tones
 * @param measured_a4_hz Measured A4 frequency
 * @param measured_a5_hz Measured A5 frequency
 * @param use_enhanced_encoding Whether transmission uses enhanced encoding
 * @return Frequency multiplier for clock rate correction (0.0 if error)
 */
float penta_calculate_frequency_multiplier(float measured_a4_hz, float measured_a5_hz, bool use_enhanced_encoding);

#endif // PENTATONIC_TX_H
