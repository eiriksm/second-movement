/*
 * Simple test program for fesk_tx library
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "fesk_tx.h"

static void test_crc16() {
    printf("Testing CRC-16 calculation... ");
    
    // Test known CRC values
    const uint8_t test_data[] = "123456789";
    uint16_t crc = fesk_crc16(test_data, 9, 0xFFFF);
    
    // Expected CRC-16/CCITT for "123456789" with init 0xFFFF is 0x29B1
    if (crc == 0x29B1) {
        printf("PASS\n");
    } else {
        printf("FAIL (got 0x%04X, expected 0x29B1)\n", crc);
    }
}

static void test_encoder_init() {
    printf("Testing encoder initialization... ");
    
    const uint8_t test_payload[] = "test";
    fesk_encoder_state_t encoder;
    
    fesk_result_t result = fesk_init_encoder(&encoder, test_payload, 4);
    
    if (result == FESK_SUCCESS) {
        printf("PASS\n");
    } else {
        printf("FAIL (result %d)\n", result);
    }
}

static void test_tone_generation() {
    printf("Testing tone generation... ");
    
    const uint8_t test_payload[] = "test";
    fesk_encoder_state_t encoder;
    
    fesk_result_t result = fesk_init_encoder(&encoder, test_payload, 4);
    if (result != FESK_SUCCESS) {
        printf("FAIL (init failed)\n");
        return;
    }
    
    int tone_count = 0;
    uint8_t tone;
    
    // Generate first few tones and verify they're in valid range
    while ((tone = fesk_get_next_tone(&encoder)) != 255 && tone_count < 50) {
        if (tone > 2) {
            printf("FAIL (invalid tone %d)\n", tone);
            return;
        }
        tone_count++;
    }
    
    if (tone_count >= 25) {  // Should generate at least 25 tones for "test" + protocol overhead
        printf("PASS (generated %d tones)\n", tone_count);
    } else {
        printf("FAIL (only generated %d tones)\n", tone_count);
    }
}

static void test_frequency_generation() {
    printf("Testing frequency generation... ");
    
    const uint8_t test_payload[] = "test";
    fesk_encoder_state_t encoder;
    
    fesk_result_t result = fesk_init_encoder(&encoder, test_payload, 4);
    if (result != FESK_SUCCESS) {
        printf("FAIL (init failed)\n");
        return;
    }
    
    uint16_t f0 = fesk_get_tone_frequency(&encoder, 0);
    uint16_t f1 = fesk_get_tone_frequency(&encoder, 1);
    uint16_t f2 = fesk_get_tone_frequency(&encoder, 2);
    
    // Check default frequencies (4:5:6 ratio)
    if (f0 == 2400 && f1 == 3000 && f2 == 3600) {
        printf("PASS (frequencies: %dHz, %dHz, %dHz)\n", f0, f1, f2);
    } else {
        printf("FAIL (got %dHz, %dHz, %dHz)\n", f0, f1, f2);
    }
}

static void test_protocol_sequence() {
    printf("Testing protocol sequence... ");
    
    const uint8_t test_payload[] = "A";  // Single byte for easier analysis
    fesk_encoder_state_t encoder;
    
    fesk_result_t result = fesk_init_encoder(&encoder, test_payload, 1);
    if (result != FESK_SUCCESS) {
        printf("FAIL (init failed)\n");
        return;
    }
    
    uint8_t tones[200];
    int tone_count = 0;
    uint8_t tone;
    
    // Collect all tones
    while ((tone = fesk_get_next_tone(&encoder)) != 255 && tone_count < 200) {
        tones[tone_count++] = tone;
    }
    
    if (tone_count < 25) {
        printf("FAIL (too few tones: %d)\n", tone_count);
        return;
    }
    
    // Check preamble (first 12 tones should be alternating 0,2)
    bool preamble_ok = true;
    for (int i = 0; i < 12; i++) {
        uint8_t expected = (i % 2 == 0) ? 2 : 0; // 1010... -> f2,f0,f2,f0...
        if (tones[i] != expected) {
            preamble_ok = false;
            break;
        }
    }
    
    if (preamble_ok) {
        printf("PASS (generated %d tones, preamble correct)\n", tone_count);
    } else {
        printf("FAIL (preamble incorrect)\n");
        printf("First 15 tones: ");
        for (int i = 0; i < 15 && i < tone_count; i++) {
            printf("%d ", tones[i]);
        }
        printf("\n");
    }
}

int main() {
    printf("=== FESK Transmission Library Unit Tests ===\n\n");
    
    test_crc16();
    test_encoder_init();
    test_tone_generation();
    test_frequency_generation();
    test_protocol_sequence();
    
    printf("\n=== Tests Complete ===\n");
    return 0;
}