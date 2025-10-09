#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "watch_tcc.h"
#include "../fesk_tx.h"
#include "unity.h"

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))
#define FESK_TICKS_PER_SYMBOL 4
#define FESK_PREAMBLE_TONE_TICKS 20
#define FESK_CRC_MASK 0x1F

static int8_t note_for_digit(uint8_t digit) {
    switch (digit) {
        case 0: return (int8_t)BUZZER_NOTE_F7;
        case 1: return (int8_t)BUZZER_NOTE_A7;
        case 2: return (int8_t)BUZZER_NOTE_D8;
        case 3: return (int8_t)BUZZER_NOTE_G6;
        default:
            TEST_FAIL_MESSAGE("Digit outside supported range");
            return 0;
    }
}

static uint8_t compute_crc_from_digits(const uint8_t *digits, size_t count) {
    uint8_t crc = 0;
    for (size_t i = 0; i < count; ++i) {
        crc = (uint8_t)((crc + digits[i]) & FESK_CRC_MASK);
    }
    return crc;
}

static void value_to_base4_digits(uint8_t value, uint8_t out[3]) {
    out[0] = (uint8_t)(value & 0x03);
    out[1] = (uint8_t)((value >> 2) & 0x03);
    out[2] = (uint8_t)((value >> 4) & 0x03);
}

static size_t assert_digit_block(const int8_t *sequence,
                                 size_t start,
                                 const uint8_t *digits,
                                 size_t digit_count) {
    for (size_t i = 0; i < digit_count; ++i) {
        size_t base = start + (i * 4);
        TEST_ASSERT_EQUAL_INT8(note_for_digit(digits[i]), sequence[base]);
        TEST_ASSERT_EQUAL_INT8(FESK_TICKS_PER_SYMBOL, sequence[base + 1]);
        TEST_ASSERT_EQUAL_INT8((int8_t)BUZZER_NOTE_REST, sequence[base + 2]);
        TEST_ASSERT_EQUAL_INT8(FESK_TICKS_PER_SYMBOL, sequence[base + 3]);
    }
    return start + (digit_count * 4);
}

static size_t assert_sequence_matches_digits(const int8_t *sequence,
                                             size_t entries,
                                             const uint8_t *payload_digits,
                                             size_t payload_digit_count) {
    static const uint8_t frame_marker_digits[] = {3, 3, 3};

    uint8_t crc_value = compute_crc_from_digits(payload_digits, payload_digit_count);
    uint8_t crc_digits[3];
    value_to_base4_digits(crc_value, crc_digits);

    size_t total_digit_count = ARRAY_LENGTH(frame_marker_digits)        // start marker
                             + payload_digit_count
                             + ARRAY_LENGTH(crc_digits)
                             + ARRAY_LENGTH(frame_marker_digits);       // end marker
    size_t expected_entries = 4 + (total_digit_count * 4);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)expected_entries, (uint32_t)entries);

    TEST_ASSERT_EQUAL_INT8(note_for_digit(0), sequence[0]);
    TEST_ASSERT_EQUAL_INT8(FESK_PREAMBLE_TONE_TICKS, sequence[1]);
    TEST_ASSERT_EQUAL_INT8((int8_t)BUZZER_NOTE_REST, sequence[2]);
    TEST_ASSERT_EQUAL_INT8(FESK_TICKS_PER_SYMBOL, sequence[3]);

    size_t pos = 4;
    pos = assert_digit_block(sequence, pos, frame_marker_digits, ARRAY_LENGTH(frame_marker_digits));
    pos = assert_digit_block(sequence, pos, payload_digits, payload_digit_count);
    pos = assert_digit_block(sequence, pos, crc_digits, ARRAY_LENGTH(crc_digits));
    pos = assert_digit_block(sequence, pos, frame_marker_digits, ARRAY_LENGTH(frame_marker_digits));

    return expected_entries;
}

void setUp(void) {
}

void tearDown(void) {
}

static void test_encode_text_basic(void) {
    const char input[] = "A1";
    const uint8_t expected_digits[] = {0, 0, 3, 3, 0, 1};
    int8_t *sequence = NULL;
    size_t entries = 0;

    fesk_result_t result = fesk_encode_text(input, sizeof(input) - 1, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);
    size_t expected_entries = assert_sequence_matches_digits(sequence, entries, expected_digits, ARRAY_LENGTH(expected_digits));
    TEST_ASSERT_EQUAL_INT8(0, sequence[expected_entries]);

    fesk_free_sequence(sequence);
}

static void test_encode_text_is_case_insensitive(void) {
    const char input[] = "Az";
    const uint8_t expected_digits[] = {0, 0, 3, 3, 1, 0};
    int8_t *sequence = NULL;
    size_t entries = 0;

    fesk_result_t result = fesk_encode_text(input, sizeof(input) - 1, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    size_t expected_entries = assert_sequence_matches_digits(sequence, entries, expected_digits, ARRAY_LENGTH(expected_digits));
    TEST_ASSERT_EQUAL_INT8(0, sequence[expected_entries]);

    fesk_free_sequence(sequence);
}

static void test_encode_text_handles_symbols(void) {
    const char input[] = " ,:";
    const uint8_t expected_digits[] = {3, 3, 1, 1, 3, 3, 1, 2, 3, 3, 1, 3};
    int8_t *sequence = NULL;
    size_t entries = 0;

    fesk_result_t result = fesk_encode_text(input, sizeof(input) - 1, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    size_t expected_entries = assert_sequence_matches_digits(sequence, entries, expected_digits, ARRAY_LENGTH(expected_digits));
    TEST_ASSERT_EQUAL_INT8(0, sequence[expected_entries]);

    fesk_free_sequence(sequence);
}

static void test_encode_text_optional_out_entries(void) {
    const char input[] = "hi";
    const uint8_t expected_digits[] = {1, 3, 2, 0};
    int8_t *sequence = NULL;

    fesk_result_t result = fesk_encode_text(input, sizeof(input) - 1, &sequence, NULL);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);
    size_t expected_entries = assert_sequence_matches_digits(sequence,
                                                             4 + (ARRAY_LENGTH(expected_digits) + 9) * 4,
                                                             expected_digits,
                                                             ARRAY_LENGTH(expected_digits));
    TEST_ASSERT_EQUAL_INT8(0, sequence[expected_entries]);

    fesk_free_sequence(sequence);
}

static void test_encode_text_rejects_unsupported_characters(void) {
    const char input[] = "?";
    int8_t sentinel = 42;
    int8_t *sequence = &sentinel;
    size_t entries = 123;

    fesk_result_t result = fesk_encode_text(input, sizeof(input) - 1, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_ERR_UNSUPPORTED_CHARACTER, result);
    TEST_ASSERT_EQUAL_PTR(&sentinel, sequence);
    TEST_ASSERT_EQUAL_UINT32(123, (uint32_t)entries);
}

static void test_encode_text_argument_validation(void) {
    int8_t sentinel = 13;
    int8_t *sequence = &sentinel;
    size_t entries = 77;

    TEST_ASSERT_EQUAL(FESK_ERR_INVALID_ARGUMENT, fesk_encode_text(NULL, 1, &sequence, &entries));
    TEST_ASSERT_EQUAL_PTR(&sentinel, sequence);
    TEST_ASSERT_EQUAL_UINT32(77, (uint32_t)entries);

    sequence = &sentinel;
    entries = 77;
    TEST_ASSERT_EQUAL(FESK_ERR_INVALID_ARGUMENT, fesk_encode_text("A", 0, &sequence, &entries));
    TEST_ASSERT_EQUAL_PTR(&sentinel, sequence);
    TEST_ASSERT_EQUAL_UINT32(77, (uint32_t)entries);

    TEST_ASSERT_EQUAL(FESK_ERR_INVALID_ARGUMENT, fesk_encode_text("A", 1, NULL, &entries));
}

static void test_encode_cstr_success(void) {
    const char *input = "fEsk";
    const uint8_t expected_digits[] = {1, 1, 1, 0, 3, 1, 2, 2, 2};
    int8_t *sequence = NULL;
    size_t entries = 0;

    fesk_result_t result = fesk_encode_cstr(input, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    size_t expected_entries = assert_sequence_matches_digits(sequence, entries, expected_digits, ARRAY_LENGTH(expected_digits));
    TEST_ASSERT_EQUAL_INT8(0, sequence[expected_entries]);

    fesk_free_sequence(sequence);
}

static void test_encode_cstr_empty_string_is_invalid(void) {
    const char *input = "";
    int8_t sentinel = 7;
    int8_t *sequence = &sentinel;
    size_t entries = 0;

    fesk_result_t result = fesk_encode_cstr(input, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_ERR_INVALID_ARGUMENT, result);
    TEST_ASSERT_EQUAL_PTR(&sentinel, sequence);
}

static void test_encode_cstr_null_pointer_is_invalid(void) {
    int8_t *sequence = NULL;
    size_t entries = 0;

    TEST_ASSERT_EQUAL(FESK_ERR_INVALID_ARGUMENT, fesk_encode_cstr(NULL, &sequence, &entries));
}

static void test_free_sequence_accepts_null(void) {
    fesk_free_sequence(NULL);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_encode_text_basic);
    RUN_TEST(test_encode_text_is_case_insensitive);
    RUN_TEST(test_encode_text_handles_symbols);
    RUN_TEST(test_encode_text_optional_out_entries);
    RUN_TEST(test_encode_text_rejects_unsupported_characters);
    RUN_TEST(test_encode_text_argument_validation);
    RUN_TEST(test_encode_cstr_success);
    RUN_TEST(test_encode_cstr_empty_string_is_invalid);
    RUN_TEST(test_encode_cstr_null_pointer_is_invalid);
    RUN_TEST(test_free_sequence_accepts_null);
    return UNITY_END();
}
