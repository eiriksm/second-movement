/*
 * MIT License
 *
 * Copyright (c) 2025 Eirik S. Morland
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

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../fesk_tx.h"
#include "unity.h"

void setUp(void) {
}

void tearDown(void) {
}

// Test basic encoding (4FSK)
void test_encode_simple_text() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    fesk_result_t result = fesk_encode("a", FESK_MODE_4FSK, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);
    TEST_ASSERT_GREATER_THAN(0, entries);

    // Clean up
    fesk_free_sequence(sequence);
}

// Test encoding with mixed case (should be case-sensitive now)
void test_encode_case_sensitive() {
    int8_t *seq_lower = NULL;
    int8_t *seq_upper = NULL;
    size_t entries_lower = 0;
    size_t entries_upper = 0;

    fesk_result_t result1 = fesk_encode("hello", FESK_MODE_4FSK, &seq_lower, &entries_lower);
    fesk_result_t result2 = fesk_encode("HELLO", FESK_MODE_4FSK, &seq_upper, &entries_upper);

    TEST_ASSERT_EQUAL(FESK_OK, result1);
    TEST_ASSERT_EQUAL(FESK_OK, result2);
    TEST_ASSERT_EQUAL(entries_lower, entries_upper);

    // Sequences should be different (case-sensitive)
    int differences = 0;
    for (size_t i = 0; i < entries_lower; i++) {
        if (seq_lower[i] != seq_upper[i]) {
            differences++;
        }
    }
    TEST_ASSERT_GREATER_THAN(0, differences);

    fesk_free_sequence(seq_lower);
    fesk_free_sequence(seq_upper);
}

// Test printable ASCII characters
void test_encode_all_characters() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    // Test printable ASCII (32-126) plus common control chars
    const char *test_str = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\t\n\r";
    fesk_result_t result = fesk_encode(test_str, FESK_MODE_4FSK, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);

    fesk_free_sequence(sequence);
}

// Test unsupported character
void test_encode_unsupported_character() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    // Character with value > 127 (extended ASCII) is not supported
    char test_str[] = "hello\xFFworld";  // \xFF = 255, outside ASCII range
    fesk_result_t result = fesk_encode(test_str, FESK_MODE_4FSK, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_ERR_UNSUPPORTED_CHARACTER, result);
    TEST_ASSERT_NULL(sequence);
}

// Test NULL pointer handling
void test_encode_null_pointer() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    fesk_result_t result = fesk_encode(NULL, FESK_MODE_4FSK, &sequence, &entries);
    TEST_ASSERT_EQUAL(FESK_ERR_INVALID_ARGUMENT, result);

    result = fesk_encode("hello", FESK_MODE_4FSK, NULL, &entries);
    TEST_ASSERT_EQUAL(FESK_ERR_INVALID_ARGUMENT, result);
}

// Test empty string
void test_encode_empty_string() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    fesk_result_t result = fesk_encode("", FESK_MODE_4FSK, &sequence, &entries);
    TEST_ASSERT_EQUAL(FESK_ERR_INVALID_ARGUMENT, result);
}

// Test sequence structure (should have start/end markers)
void test_sequence_structure() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    fesk_result_t result = fesk_encode("a", FESK_MODE_4FSK, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);

    // Sequence should be null-terminated
    TEST_ASSERT_EQUAL(0, sequence[entries]);

    // Each dibit (2 bits) is encoded as: [TONE, TICKS, REST, TICKS]
    // So entries should be a multiple of 4
    // Format: START(4 dibits) + 'a'(4 dibits) + CRC(4 dibits) + END(4 dibits) = 16 dibits
    // 16 dibits * 4 entries/dibit = 64 entries
    TEST_ASSERT_EQUAL(64, entries);

    fesk_free_sequence(sequence);
}

// Test digits encoding
void test_encode_digits() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    fesk_result_t result = fesk_encode("0123456789", FESK_MODE_4FSK, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);

    fesk_free_sequence(sequence);
}

// Test punctuation
void test_encode_punctuation() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    const char *test_str = "hello, world: 'test' \"quote\"";
    fesk_result_t result = fesk_encode(test_str, FESK_MODE_4FSK, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);

    fesk_free_sequence(sequence);
}

// Test newline character
void test_encode_newline() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    const char *test_str = "line1\nline2";
    fesk_result_t result = fesk_encode(test_str, FESK_MODE_4FSK, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);

    fesk_free_sequence(sequence);
}

// Test maximum length string
void test_encode_max_length() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    // Create a 1024 character string (max allowed)
    char long_str[1025];
    memset(long_str, 'a', 1024);
    long_str[1024] = '\0';

    fesk_result_t result = fesk_encode(long_str, FESK_MODE_4FSK, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);

    fesk_free_sequence(sequence);
}

// Test over maximum length string (should fail)
void test_encode_over_max_length() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    // Create a 1025 character string (over max)
    char long_str[1026];
    memset(long_str, 'a', 1025);
    long_str[1025] = '\0';

    fesk_result_t result = fesk_encode(long_str, FESK_MODE_4FSK, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_ERR_INVALID_ARGUMENT, result);
    TEST_ASSERT_NULL(sequence);
}

// Test tone mapping (4-FSK)
void test_tone_mapping() {
    // Verify 4 tones are correctly mapped
    TEST_ASSERT_EQUAL(FESK_TONE_00_NOTE, fesk_tone_map[FESK_TONE_00]);
    TEST_ASSERT_EQUAL(FESK_TONE_01_NOTE, fesk_tone_map[FESK_TONE_01]);
    TEST_ASSERT_EQUAL(FESK_TONE_10_NOTE, fesk_tone_map[FESK_TONE_10]);
    TEST_ASSERT_EQUAL(FESK_TONE_11_NOTE, fesk_tone_map[FESK_TONE_11]);
    TEST_ASSERT_EQUAL(4, FESK_TONE_COUNT);
}

// Test free with NULL (should be safe)
void test_free_null_sequence() {
    fesk_free_sequence(NULL);  // Should not crash
    TEST_PASS();
}

// Test encoding produces different sequences for different inputs
void test_different_inputs_different_sequences() {
    int8_t *seq1 = NULL;
    int8_t *seq2 = NULL;
    size_t entries1 = 0;
    size_t entries2 = 0;

    fesk_encode("abc", FESK_MODE_4FSK, &seq1, &entries1);
    fesk_encode("xyz", FESK_MODE_4FSK, &seq2, &entries2);

    TEST_ASSERT_EQUAL(entries1, entries2);  // Same length

    // But sequences should differ (comparing some middle portion to avoid start/end markers)
    int differences = 0;
    for (size_t i = 20; i < entries1 - 20; i++) {
        if (seq1[i] != seq2[i]) {
            differences++;
        }
    }
    TEST_ASSERT_GREATER_THAN(0, differences);

    fesk_free_sequence(seq1);
    fesk_free_sequence(seq2);
}

// Test 2FSK basic encoding
void test_encode_2fsk_simple() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    fesk_result_t result = fesk_encode("a", FESK_MODE_2FSK, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);
    TEST_ASSERT_GREATER_THAN(0, entries);

    fesk_free_sequence(sequence);
}

// Test 2FSK vs 4FSK sequence lengths
void test_2fsk_vs_4fsk_sequence_length() {
    int8_t *seq_2fsk = NULL;
    int8_t *seq_4fsk = NULL;
    size_t entries_2fsk = 0;
    size_t entries_4fsk = 0;

    fesk_encode("a", FESK_MODE_2FSK, &seq_2fsk, &entries_2fsk);
    fesk_encode("a", FESK_MODE_4FSK, &seq_4fsk, &entries_4fsk);

    // 2FSK should produce twice as many symbols as 4FSK
    // 4FSK: START(4) + 'a'(4) + CRC(4) + END(4) = 16 dibits = 64 entries
    // 2FSK: START(8) + 'a'(8) + CRC(8) + END(8) = 32 bits = 128 entries
    TEST_ASSERT_EQUAL(64, entries_4fsk);
    TEST_ASSERT_EQUAL(128, entries_2fsk);
    TEST_ASSERT_EQUAL(entries_4fsk * 2, entries_2fsk);

    fesk_free_sequence(seq_2fsk);
    fesk_free_sequence(seq_4fsk);
}

// Test 2FSK tone mapping
void test_2fsk_tone_mapping() {
    TEST_ASSERT_EQUAL(FESK_2FSK_TONE_0_NOTE, fesk_tone_map_2fsk[FESK_2FSK_TONE_0]);
    TEST_ASSERT_EQUAL(FESK_2FSK_TONE_1_NOTE, fesk_tone_map_2fsk[FESK_2FSK_TONE_1]);
    TEST_ASSERT_EQUAL(2, FESK_2FSK_TONE_COUNT);
}

// Test 2FSK with all supported characters
void test_encode_2fsk_all_characters() {
    int8_t *sequence = NULL;
    size_t entries = 0;

    // Test printable ASCII (32-126) plus common control chars
    const char *test_str = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\t\n\r";
    fesk_result_t result = fesk_encode(test_str, FESK_MODE_2FSK, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);

    fesk_free_sequence(sequence);
}

// Test 2FSK case sensitivity
void test_encode_2fsk_case_sensitive() {
    int8_t *seq_lower = NULL;
    int8_t *seq_upper = NULL;
    size_t entries_lower = 0;
    size_t entries_upper = 0;

    fesk_encode("hello", FESK_MODE_2FSK, &seq_lower, &entries_lower);
    fesk_encode("HELLO", FESK_MODE_2FSK, &seq_upper, &entries_upper);

    TEST_ASSERT_EQUAL(entries_lower, entries_upper);

    // Sequences should be different (case-sensitive)
    int differences = 0;
    for (size_t i = 0; i < entries_lower; i++) {
        if (seq_lower[i] != seq_upper[i]) {
            differences++;
        }
    }
    TEST_ASSERT_GREATER_THAN(0, differences);

    fesk_free_sequence(seq_lower);
    fesk_free_sequence(seq_upper);
}

int main(void) {
    UNITY_BEGIN();

    // Basic functionality (4FSK)
    RUN_TEST(test_encode_simple_text);
    RUN_TEST(test_encode_case_sensitive);
    RUN_TEST(test_encode_all_characters);

    // Character support
    RUN_TEST(test_encode_digits);
    RUN_TEST(test_encode_punctuation);
    RUN_TEST(test_encode_newline);

    // Error cases
    RUN_TEST(test_encode_unsupported_character);
    RUN_TEST(test_encode_null_pointer);
    RUN_TEST(test_encode_empty_string);

    // Edge cases
    RUN_TEST(test_encode_max_length);
    RUN_TEST(test_encode_over_max_length);
    RUN_TEST(test_free_null_sequence);

    // Structure and correctness (4FSK)
    RUN_TEST(test_sequence_structure);
    RUN_TEST(test_tone_mapping);
    RUN_TEST(test_different_inputs_different_sequences);

    // 2FSK mode tests
    RUN_TEST(test_encode_2fsk_simple);
    RUN_TEST(test_2fsk_vs_4fsk_sequence_length);
    RUN_TEST(test_2fsk_tone_mapping);
    RUN_TEST(test_encode_2fsk_all_characters);
    RUN_TEST(test_encode_2fsk_case_sensitive);

    return UNITY_END();
}
