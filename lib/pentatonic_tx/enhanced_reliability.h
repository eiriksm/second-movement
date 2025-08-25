/*
 * MIT License
 *
 * Copyright (c) 2024 Second Movement Project
 *
 * Enhanced reliability schemes for pentatonic transmission
 */

#ifndef ENHANCED_RELIABILITY_H
#define ENHANCED_RELIABILITY_H

#include <stdint.h>
#include <stdbool.h>

/** @brief Enhanced encoding schemes for better noise resistance
 */
typedef enum {
    PENTA_ENCODING_ORIGINAL,     // 3 bits per tone (current)
    PENTA_ENCODING_2BIT_SPREAD,  // 2 bits per tone, max frequency spacing
    PENTA_ENCODING_1BIT_OCTAVE,  // 1 bit per tone, octave separation
    PENTA_ENCODING_DIFFERENTIAL, // Encode transitions, not absolute values
    PENTA_ENCODING_VOTING,       // Triple repetition with majority vote
} penta_encoding_scheme_t;

/** @brief Repetition schemes for error correction
 */
typedef enum {
    PENTA_REPEAT_NONE,          // No repetition
    PENTA_REPEAT_TRIPLE,        // Send each tone 3 times, majority vote
    PENTA_REPEAT_INTERLEAVED,   // Interleave repetitions across time
    PENTA_REPEAT_HAMMING_74,    // 7,4 Hamming code repetition
} penta_repetition_scheme_t;

/** @brief Enhanced reliability configuration
 */
typedef struct {
    penta_encoding_scheme_t encoding;
    penta_repetition_scheme_t repetition;
    
    // Timing parameters for better detection
    uint16_t tone_duration_ms;      // Longer = more reliable
    uint16_t silence_duration_ms;   // Longer = better separation
    uint16_t guard_time_ms;         // Extra silence between symbols
    
    // Frequency parameters
    bool use_wider_spacing;         // Use every other frequency
    bool enable_frequency_hopping;  // Change frequencies to avoid interference
    
    // Detection enhancement
    bool enable_tone_ramping;       // Gradual volume changes to reduce clicks
    bool enable_sync_patterns;      // Regular sync tones for timing recovery
    
} enhanced_reliability_config_t;

// Enhanced frequency tables for better separation
extern const uint16_t penta_frequencies_wide_spacing[4];   // 2-bit encoding
extern const uint16_t penta_frequencies_octave[2];        // 1-bit encoding
extern const uint16_t penta_frequencies_differential[6];  // For transitions

// API functions
void enhanced_reliability_init_config(enhanced_reliability_config_t *config, 
                                     penta_encoding_scheme_t encoding,
                                     penta_repetition_scheme_t repetition);

uint8_t enhanced_reliability_encode_byte(uint8_t data_byte, 
                                        const enhanced_reliability_config_t *config,
                                        uint8_t *tone_buffer, 
                                        uint8_t max_tones);

bool enhanced_reliability_validate_config(const enhanced_reliability_config_t *config);

// Voting and error correction
uint8_t majority_vote_3(uint8_t val1, uint8_t val2, uint8_t val3);
uint8_t hamming_encode_4_to_7(uint8_t nibble);
int8_t hamming_decode_7_to_4(uint8_t received, uint8_t *corrected);

#endif // ENHANCED_RELIABILITY_H


