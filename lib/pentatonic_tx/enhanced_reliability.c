/*
 * MIT License
 *
 * Copyright (c) 2024 Second Movement Project
 *
 * Enhanced reliability schemes for pentatonic transmission
 */

#include "enhanced_reliability.h"
#include <string.h>

// Wide-spaced frequencies for 2-bit encoding (every other pentatonic note)
const uint16_t penta_frequencies_wide_spacing[4] = {
    440,   // A4  - 00
    660,   // E5  - 01 (skip B4, C#5)
    880,   // A5  - 10 (skip F#5, G#5) 
    1320   // E6  - 11 (skip B5, C#6)
};

// Octave-separated frequencies for 1-bit encoding (maximum separation)
const uint16_t penta_frequencies_octave[2] = {
    440,   // A4 - bit 0
    880    // A5 - bit 1 (perfect octave, very distinguishable)
};

// Extended range for differential encoding
const uint16_t penta_frequencies_differential[6] = {
    330,   // E4  (lower than normal range)
    440,   // A4
    660,   // E5  
    880,   // A5
    1320,  // E6
    1760   // A6  (higher than normal range)
};

void enhanced_reliability_init_config(enhanced_reliability_config_t *config, 
                                     penta_encoding_scheme_t encoding,
                                     penta_repetition_scheme_t repetition) {
    if (!config) return;
    
    memset(config, 0, sizeof(enhanced_reliability_config_t));
    config->encoding = encoding;
    config->repetition = repetition;
    
    // Set defaults based on encoding scheme
    switch (encoding) {
        case PENTA_ENCODING_ORIGINAL:
            config->tone_duration_ms = 30;
            config->silence_duration_ms = 10;
            config->guard_time_ms = 5;
            break;
            
        case PENTA_ENCODING_2BIT_SPREAD:
            config->tone_duration_ms = 40;
            config->silence_duration_ms = 15;
            config->guard_time_ms = 10;
            config->use_wider_spacing = true;
            break;
            
        case PENTA_ENCODING_1BIT_OCTAVE:
            config->tone_duration_ms = 50;
            config->silence_duration_ms = 20;
            config->guard_time_ms = 15;
            config->use_wider_spacing = true;
            config->enable_tone_ramping = true;
            break;
            
        case PENTA_ENCODING_DIFFERENTIAL:
            config->tone_duration_ms = 60;
            config->silence_duration_ms = 25;
            config->guard_time_ms = 20;
            config->enable_sync_patterns = true;
            break;
            
        case PENTA_ENCODING_VOTING:
            config->tone_duration_ms = 35;
            config->silence_duration_ms = 12;
            config->guard_time_ms = 8;
            break;
    }
    
    // Adjust for repetition scheme
    switch (repetition) {
        case PENTA_REPEAT_NONE:
            break;
            
        case PENTA_REPEAT_TRIPLE:
            // Shorter individual tones since we repeat 3x
            config->tone_duration_ms = config->tone_duration_ms * 2 / 3;
            config->silence_duration_ms = config->silence_duration_ms * 2 / 3;
            break;
            
        case PENTA_REPEAT_INTERLEAVED:
            // Longer gaps for interleaving
            config->guard_time_ms = config->guard_time_ms * 2;
            break;
            
        case PENTA_REPEAT_HAMMING_74:
            // Balanced timing for Hamming codes
            config->tone_duration_ms = config->tone_duration_ms * 3 / 4;
            break;
    }
}

uint8_t enhanced_reliability_encode_byte(uint8_t data_byte, 
                                        const enhanced_reliability_config_t *config,
                                        uint8_t *tone_buffer, 
                                        uint8_t max_tones) {
    if (!config || !tone_buffer || max_tones == 0) return 0;
    
    uint8_t tone_count = 0;
    
    switch (config->encoding) {
        case PENTA_ENCODING_ORIGINAL:
            // 3 bits per tone (8 data tones + silence/control)
            if (max_tones < 3) return 0;
            
            // Split byte into 3-bit chunks: AAABBBCC -> AAA, BBB, CC0
            tone_buffer[tone_count++] = (data_byte >> 5) & 0x07;        // Top 3 bits
            tone_buffer[tone_count++] = (data_byte >> 2) & 0x07;        // Middle 3 bits  
            tone_buffer[tone_count++] = ((data_byte & 0x03) << 1);      // Bottom 2 bits + 0
            break;
            
        case PENTA_ENCODING_2BIT_SPREAD:
            // 2 bits per tone with wide frequency spacing
            if (max_tones < 4) return 0;
            
            for (int i = 0; i < 4; i++) {
                tone_buffer[tone_count++] = (data_byte >> (6 - i*2)) & 0x03;
            }
            break;
            
        case PENTA_ENCODING_1BIT_OCTAVE:
            // 1 bit per tone, maximum reliability
            if (max_tones < 8) return 0;
            
            for (int i = 0; i < 8; i++) {
                tone_buffer[tone_count++] = (data_byte >> (7 - i)) & 0x01;
            }
            break;
            
        case PENTA_ENCODING_DIFFERENTIAL:
            // Encode changes between frequencies
            if (max_tones < 9) return 0; // Need start frequency + 8 transitions
            
            tone_buffer[tone_count++] = 2; // Start at middle frequency (index 2)
            
            for (int i = 0; i < 8; i++) {
                uint8_t bit = (data_byte >> (7 - i)) & 0x01;
                uint8_t prev_tone = tone_buffer[tone_count - 1];
                
                if (bit == 1) {
                    // Go up in frequency (but stay in range)
                    tone_buffer[tone_count++] = (prev_tone < 5) ? prev_tone + 1 : prev_tone;
                } else {
                    // Go down in frequency (but stay in range)  
                    tone_buffer[tone_count++] = (prev_tone > 0) ? prev_tone - 1 : prev_tone;
                }
            }
            break;
            
        case PENTA_ENCODING_VOTING:
            // Triple repetition for majority voting
            if (max_tones < 9) return 0; // 3 tones × 3 repetitions
            
            // First encode normally
            uint8_t base_tones[3];
            base_tones[0] = (data_byte >> 5) & 0x07;
            base_tones[1] = (data_byte >> 2) & 0x07;  
            base_tones[2] = ((data_byte & 0x03) << 1);
            
            // Then repeat each tone 3 times
            for (int tone = 0; tone < 3; tone++) {
                for (int rep = 0; rep < 3; rep++) {
                    if (tone_count < max_tones) {
                        tone_buffer[tone_count++] = base_tones[tone];
                    }
                }
            }
            break;
    }
    
    return tone_count;
}

bool enhanced_reliability_validate_config(const enhanced_reliability_config_t *config) {
    if (!config) return false;
    
    if (config->tone_duration_ms < 10 || config->tone_duration_ms > 500) return false;
    if (config->silence_duration_ms > 200) return false;
    if (config->guard_time_ms > 100) return false;
    
    return true;
}

// Majority voting for triple repetition
uint8_t majority_vote_3(uint8_t val1, uint8_t val2, uint8_t val3) {
    if (val1 == val2) return val1;
    if (val1 == val3) return val1;
    if (val2 == val3) return val2;
    
    // No majority, return first value as fallback
    return val1;
}

// Hamming (7,4) encoding: 4 data bits -> 7 bits with error correction
uint8_t hamming_encode_4_to_7(uint8_t nibble) {
    nibble &= 0x0F; // Ensure only 4 bits
    
    // Calculate parity bits
    uint8_t p1 = ((nibble >> 0) ^ (nibble >> 1) ^ (nibble >> 3)) & 1;
    uint8_t p2 = ((nibble >> 0) ^ (nibble >> 2) ^ (nibble >> 3)) & 1;  
    uint8_t p4 = ((nibble >> 1) ^ (nibble >> 2) ^ (nibble >> 3)) & 1;
    
    // Pack into 7-bit codeword: p1 p2 d1 p4 d2 d3 d4
    return (p1 << 6) | (p2 << 5) | ((nibble & 1) << 4) | (p4 << 3) | 
           (((nibble >> 1) & 1) << 2) | (((nibble >> 2) & 1) << 1) | ((nibble >> 3) & 1);
}

// Hamming (7,4) decoding with single error correction
int8_t hamming_decode_7_to_4(uint8_t received, uint8_t *corrected) {
    if (!corrected) return -1;
    
    // Extract bits
    uint8_t p1 = (received >> 6) & 1;
    uint8_t p2 = (received >> 5) & 1;
    uint8_t d1 = (received >> 4) & 1;
    uint8_t p4 = (received >> 3) & 1;
    uint8_t d2 = (received >> 2) & 1;
    uint8_t d3 = (received >> 1) & 1;
    uint8_t d4 = received & 1;
    
    // Calculate syndrome
    uint8_t s1 = p1 ^ d1 ^ d2 ^ d4;
    uint8_t s2 = p2 ^ d1 ^ d3 ^ d4;
    uint8_t s4 = p4 ^ d2 ^ d3 ^ d4;
    uint8_t syndrome = (s4 << 2) | (s2 << 1) | s1;
    
    // Correct single error if present
    if (syndrome != 0) {
        // Flip the erroneous bit
        received ^= (1 << (7 - syndrome));
        
        // Re-extract corrected data bits
        d1 = (received >> 4) & 1;
        d2 = (received >> 2) & 1;
        d3 = (received >> 1) & 1;
        d4 = received & 1;
    }
    
    // Extract 4-bit data
    *corrected = (d4 << 3) | (d3 << 2) | (d2 << 1) | d1;
    
    return (syndrome == 0) ? 0 : 1; // Return number of errors corrected
}


