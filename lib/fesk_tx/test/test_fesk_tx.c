#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "watch_tcc.h"
#include "../fesk_tx.h"
#include "unity.h"

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))
#define FESK_TICKS_PER_BIT 4
#define FESK_TICKS_PER_REST 4
#define FESK_BITS_PER_CODE 6
#define FESK_START_MARKER 62u
#define FESK_END_MARKER 63u

static int8_t note_for_bit(uint8_t bit) {
    return bit ? (int8_t)BUZZER_NOTE_G7 : (int8_t)BUZZER_NOTE_D7SHARP_E7FLAT;
}

static uint8_t crc8_update_bit(uint8_t crc, uint8_t bit) {
    uint8_t mix = ((crc >> 7) & 0x01u) ^ (bit & 0x01u);
    crc <<= 1;
    if (mix) {
        crc ^= 0x07u;
    }
    return crc;
}

static uint8_t crc8_update_code(uint8_t crc, uint8_t code) {
    for (int shift = FESK_BITS_PER_CODE - 1; shift >= 0; --shift) {
        uint8_t bit = (uint8_t)((code >> shift) & 0x01u);
        crc = crc8_update_bit(crc, bit);
    }
    return crc;
}

static size_t bit_capacity_for_payload(size_t payload_count) {
    return FESK_BITS_PER_CODE                  // start
         + (payload_count * FESK_BITS_PER_CODE)
         + 8                                   // CRC
         + FESK_BITS_PER_CODE;                 // end
}

static void append_code_bits(uint8_t code, uint8_t *bits, size_t *bit_index) {
    for (int shift = FESK_BITS_PER_CODE - 1; shift >= 0; --shift) {
        bits[(*bit_index)++] = (uint8_t)((code >> shift) & 0x01u);
    }
}

static size_t build_expected_bits(const uint8_t *payload_codes,
                                  size_t payload_count,
                                  uint8_t *out_bits,
                                  size_t capacity) {
    size_t required = bit_capacity_for_payload(payload_count);
    TEST_ASSERT_TRUE(required <= capacity);

    size_t bit_index = 0;
    append_code_bits(FESK_START_MARKER, out_bits, &bit_index);

    uint8_t crc = 0;
    for (size_t i = 0; i < payload_count; ++i) {
        append_code_bits(payload_codes[i], out_bits, &bit_index);
        crc = crc8_update_code(crc, payload_codes[i]);
    }

    for (int shift = 7; shift >= 0; --shift) {
        out_bits[bit_index++] = (uint8_t)((crc >> shift) & 0x01u);
    }

    append_code_bits(FESK_END_MARKER, out_bits, &bit_index);
    return bit_index;
}

static size_t assert_sequence_matches_bits(const int8_t *sequence,
                                           size_t entries,
                                           const uint8_t *bits,
                                           size_t bit_count) {
    size_t expected_entries = bit_count * 4;
    TEST_ASSERT_EQUAL_UINT32((uint32_t)expected_entries, (uint32_t)entries);

    for (size_t i = 0; i < bit_count; ++i) {
        size_t base = i * 4;
        TEST_ASSERT_EQUAL_INT8(note_for_bit(bits[i]), sequence[base]);
        TEST_ASSERT_EQUAL_INT8(FESK_TICKS_PER_BIT, sequence[base + 1]);
        TEST_ASSERT_EQUAL_INT8((int8_t)BUZZER_NOTE_REST, sequence[base + 2]);
        TEST_ASSERT_EQUAL_INT8(FESK_TICKS_PER_REST, sequence[base + 3]);
    }

    return expected_entries;
}

void setUp(void) {
}

void tearDown(void) {
}

static void test_encode_text_basic(void) {
    const char input[] = "A1";
    const uint8_t payload_codes[] = {0, 27};
    uint8_t expected_bits[bit_capacity_for_payload(ARRAY_LENGTH(payload_codes))];
    size_t bit_count = build_expected_bits(payload_codes,
                                           ARRAY_LENGTH(payload_codes),
                                           expected_bits,
                                           ARRAY_LENGTH(expected_bits));

    int8_t *sequence = NULL;
    size_t entries = 0;
    fesk_result_t result = fesk_encode_text(input, sizeof(input) - 1, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);
    size_t expected_entries = assert_sequence_matches_bits(sequence, entries, expected_bits, bit_count);
    TEST_ASSERT_EQUAL_INT8(0, sequence[expected_entries]);

    fesk_free_sequence(sequence);
}

static void test_encode_text_is_case_insensitive(void) {
    const char input[] = "Az";
    const uint8_t payload_codes[] = {0, 25};
    uint8_t expected_bits[bit_capacity_for_payload(ARRAY_LENGTH(payload_codes))];
    size_t bit_count = build_expected_bits(payload_codes,
                                           ARRAY_LENGTH(payload_codes),
                                           expected_bits,
                                           ARRAY_LENGTH(expected_bits));

    int8_t *sequence = NULL;
    size_t entries = 0;
    fesk_result_t result = fesk_encode_text(input, sizeof(input) - 1, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    size_t expected_entries = assert_sequence_matches_bits(sequence, entries, expected_bits, bit_count);
    TEST_ASSERT_EQUAL_INT8(0, sequence[expected_entries]);

    fesk_free_sequence(sequence);
}

static void test_encode_text_handles_symbols(void) {
    const char input[] = " ,:";
    const uint8_t payload_codes[] = {36, 37, 38};
    uint8_t expected_bits[bit_capacity_for_payload(ARRAY_LENGTH(payload_codes))];
    size_t bit_count = build_expected_bits(payload_codes,
                                           ARRAY_LENGTH(payload_codes),
                                           expected_bits,
                                           ARRAY_LENGTH(expected_bits));

    int8_t *sequence = NULL;
    size_t entries = 0;
    fesk_result_t result = fesk_encode_text(input, sizeof(input) - 1, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    size_t expected_entries = assert_sequence_matches_bits(sequence, entries, expected_bits, bit_count);
    TEST_ASSERT_EQUAL_INT8(0, sequence[expected_entries]);

    fesk_free_sequence(sequence);
}

static void test_encode_text_optional_out_entries(void) {
    const char input[] = "hi";
    const uint8_t payload_codes[] = {7, 8};
    uint8_t expected_bits[bit_capacity_for_payload(ARRAY_LENGTH(payload_codes))];
    size_t bit_count = build_expected_bits(payload_codes,
                                           ARRAY_LENGTH(payload_codes),
                                           expected_bits,
                                           ARRAY_LENGTH(expected_bits));
    size_t expected_entries = bit_count * 4;

    int8_t *sequence = NULL;
    fesk_result_t result = fesk_encode_text(input, sizeof(input) - 1, &sequence, NULL);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    TEST_ASSERT_NOT_NULL(sequence);
    assert_sequence_matches_bits(sequence, expected_entries, expected_bits, bit_count);
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
    const uint8_t payload_codes[] = {5, 4, 18, 10};
    uint8_t expected_bits[bit_capacity_for_payload(ARRAY_LENGTH(payload_codes))];
    size_t bit_count = build_expected_bits(payload_codes,
                                           ARRAY_LENGTH(payload_codes),
                                           expected_bits,
                                           ARRAY_LENGTH(expected_bits));

    int8_t *sequence = NULL;
    size_t entries = 0;
    fesk_result_t result = fesk_encode_cstr(input, &sequence, &entries);

    TEST_ASSERT_EQUAL(FESK_OK, result);
    size_t expected_entries = assert_sequence_matches_bits(sequence, entries, expected_bits, bit_count);
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
