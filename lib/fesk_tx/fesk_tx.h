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

#ifndef FESK_TX_H
#define FESK_TX_H

#include <stdint.h>
#include <stdbool.h>

/** @brief Harmonic Triad 3-FSK Acoustic Protocol (HT3)
 * @details A robust acoustic data transmission protocol using consonant 4:5:6 major triad
 *          frequencies for improved reliability and pleasant sound. Uses 3-FSK modulation
 *          with musical harmony relationships.
 */

// Protocol configuration
#define FESK_TONE_COUNT 3              // Number of data tones (0-2)
#define FESK_PREAMBLE_LEN 12           // Binary preamble length (bits)
#define FESK_SYNC_LEN 13               // Barker-13 sync sequence length
#define FESK_HEADER_LEN 2              // Header length in bytes (payload length)
#define FESK_CRC_LEN 2                 // CRC-16 length in bytes
#define FESK_SYMBOL_TICKS 6            // Default symbol duration in 64Hz ticks (93.75ms)
#define FESK_PILOT_INTERVAL 64         // Insert pilot every N trits
#define FESK_MAX_PAYLOAD_SIZE 256      // Maximum payload size in bytes

// Safe max number of trits for a full frame (payload+header+CRC)
#define FESK_MAX_TRITS 600

// Tone frequencies (4:5:6 major triad ratios)
// Default frequencies centered around 3kHz for good piezo response
#define FESK_BASE_FREQ 2400            // Base frequency (f0 = 4k)
#define FESK_F0 2400                   // Low tone  (4k)
#define FESK_F1 3000                   // Mid tone  (5k)
#define FESK_F2 3600                   // High tone (6k)

// Alternative frequency set centered at 2.7kHz
#define FESK_ALT_F0 2700               // Low tone
#define FESK_ALT_F1 3375               // Mid tone
#define FESK_ALT_F2 4050               // High tone

/** @brief Transmission result codes
 */
typedef enum {
    FESK_SUCCESS = 0,
    FESK_ERROR_INVALID_PARAM,
    FESK_ERROR_BUFFER_FULL,
    FESK_ERROR_PAYLOAD_TOO_LARGE,
    FESK_ERROR_NO_DATA
} fesk_result_t;

/** @brief Configuration for fesk transmission
 */
typedef struct {
    uint16_t f0, f1, f2;              // Tone frequencies in Hz
    uint8_t symbol_ticks;              // Symbol duration in 64Hz ticks
    bool use_scrambler;                // Enable LFSR scrambler
    bool use_fec;                      // Enable forward error correction (future)
    bool insert_pilots;                // Insert pilot symbols for timing refresh
} fesk_config_t;

/** @brief Callback function to get next byte of data
 * @param next_byte Pointer where next byte should be written
 * @return 1 if byte available, 0 if no more data
 */
typedef uint8_t (*fesk_get_next_byte_t)(uint8_t *next_byte);

/** @brief Internal encoder state - do not manipulate directly
 */
typedef struct {
    // Configuration
    fesk_config_t config;
    fesk_get_next_byte_t get_next_byte;

    // Current transmission state
    enum {
        FESK_STATE_PREAMBLE,
        FESK_STATE_SYNC,
        FESK_STATE_HEADER,
        FESK_STATE_PAYLOAD,
        FESK_STATE_CRC,
        FESK_STATE_COMPLETE
    } state;

    // Sequence counters
    uint16_t sequence_pos;
    uint16_t bit_pos;

    // Data buffers
    uint8_t payload_buffer[FESK_MAX_PAYLOAD_SIZE];
    uint16_t payload_len;
    uint16_t payload_pos;

    // Base-3 packing state
    uint32_t trit_accumulator;
    uint8_t trits_in_accumulator;
    uint32_t byte_accumulator;
    uint8_t bytes_in_accumulator;

    // Pilot insertion
    uint16_t trit_count;

    // CRC calculation
    uint16_t crc16;

    // LFSR scrambler state
    uint16_t lfsr_state;

    // Transmission status
    bool transmission_active;

    // Tone periods for PWM (computed from frequencies)
    uint16_t tone_periods[3];

    // --- for MS-first radix-3 packing ---
    uint8_t  pack_work[FESK_MAX_PAYLOAD_SIZE + 4]; // bytes accumulated so far
    uint16_t pack_len;                              // number of valid bytes in pack_work

    uint8_t  pilot_phase;   // 0 or 1
    // Prebuilt trit stream (canonical MS-first)
    uint8_t  trit_stream[FESK_MAX_TRITS]; // size ≈ ceil((len+4) * 5.05) ; make it safe
    uint16_t trit_len;
    uint16_t trit_pos;
    uint16_t data_trit_since_pilot; // counts data trits since last pilot (optional)

} fesk_encoder_state_t;

// Core API Functions

/** @brief Initialize fesk encoder with default configuration
 * @param encoder Pointer to encoder state structure
 * @param payload_data Pointer to payload data to transmit
 * @param payload_len Length of payload data in bytes
 * @return FESK_SUCCESS or error code
 */
fesk_result_t fesk_init_encoder(fesk_encoder_state_t *encoder,
                                const uint8_t *payload_data,
                                uint16_t payload_len);

/** @brief Initialize with custom configuration
 * @param encoder Pointer to encoder state structure
 * @param config Pointer to configuration structure
 * @param payload_data Pointer to payload data to transmit
 * @param payload_len Length of payload data in bytes
 * @return FESK_SUCCESS or error code
 */
fesk_result_t fesk_init_encoder_with_config(fesk_encoder_state_t *encoder,
                                            const fesk_config_t *config,
                                            const uint8_t *payload_data,
                                            uint16_t payload_len);

/** @brief Get the next tone to transmit
 * @param encoder Pointer to encoder state
 * @return Tone index (0-2), or 255 if transmission complete
 */
uint8_t fesk_get_next_tone(fesk_encoder_state_t *encoder);

/** @brief Get the period value for a given tone (for buzzer control)
 * @param encoder Pointer to encoder state
 * @param tone_index Tone index (0-2)
 * @return Period value for 1MHz clock (period = 1,000,000 / frequency)
 */
uint16_t fesk_get_tone_period(const fesk_encoder_state_t *encoder, uint8_t tone_index);

/** @brief Get the frequency for a given tone
 * @param encoder Pointer to encoder state
 * @param tone_index Tone index (0-2)
 * @return Frequency in Hz
 */
uint16_t fesk_get_tone_frequency(const fesk_encoder_state_t *encoder, uint8_t tone_index);

/** @brief Check if transmission is still active
 * @param encoder Pointer to encoder state
 * @return True if actively transmitting
 */
bool fesk_is_transmitting(const fesk_encoder_state_t *encoder);

/** @brief Abort current transmission
 * @param encoder Pointer to encoder state
 */
void fesk_abort_transmission(fesk_encoder_state_t *encoder);

// Configuration helpers

/** @brief Get default configuration
 * @param config Pointer to config structure to populate
 */
void fesk_get_default_config(fesk_config_t *config);

/** @brief Validate configuration parameters
 * @param config Pointer to configuration to validate
 * @return FESK_SUCCESS if valid, error code if invalid
 */
fesk_result_t fesk_validate_config(const fesk_config_t *config);

// Utility functions

/** @brief Calculate CRC-16/CCITT for data
 * @param data Pointer to data
 * @param length Number of bytes
 * @param init_value Initial CRC value (typically 0xFFFF)
 * @return CRC-16 value
 */
uint16_t fesk_crc16(const uint8_t *data, uint16_t length, uint16_t init_value);

/** @brief Update CRC-16 with single byte
 * @param crc Current CRC value
 * @param byte_val Byte to add to CRC
 * @return Updated CRC value
 */
uint16_t fesk_update_crc16(uint16_t crc, uint8_t byte_val);

#endif // FESK_TX_H
