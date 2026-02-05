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
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "../fesk_session.h"
#include "../fesk_tx.h"
#include "unity.h"

// Mock watch functions
void watch_display_text(uint8_t position, const char *text) {
    (void)position;
    (void)text;
}

void watch_set_indicator(uint8_t indicator) {
    (void)indicator;
}

void watch_clear_indicator(uint8_t indicator) {
    (void)indicator;
}

void watch_buzzer_abort_sequence(void) {
}

void watch_set_buzzer_off(void) {
}

typedef bool (*mock_raw_source_t)(uint16_t position, void* userdata, uint16_t* period, uint16_t* duration);
static mock_raw_source_t captured_raw_source = NULL;
static void* captured_userdata = NULL;

void watch_buzzer_play_raw_source(bool (*raw_source)(uint16_t, void*, uint16_t*, uint16_t*),
                                   void* userdata,
                                   void (*done_callback)(void)) {
    // Capture the callback for inspection
    captured_raw_source = raw_source;
    captured_userdata = userdata;
    (void)done_callback;
}

void watch_buzzer_play_sequence(int8_t *sequence, void (*done_callback)(void)) {
    (void)sequence;
    (void)done_callback;
}

void setUp(void) {
    captured_raw_source = NULL;
    captured_userdata = NULL;
}

void tearDown(void) {
}

// Test that auto_base32_encode defaults to true
void test_config_defaults_auto_base32_encode(void) {
    fesk_session_config_t config = fesk_session_config_defaults();
    TEST_ASSERT_TRUE(config.auto_base32_encode);
}

// Test basic emoji encoding
void test_session_emoji_encoding(void) {
    fesk_session_t session;
    fesk_session_config_t config = fesk_session_config_defaults();

    // Use emoji in the message
    config.static_message = "🎵🎶";  // Musical notes emojis
    config.enable_countdown = false;  // Skip countdown for simpler testing

    fesk_session_init(&session, &config);

    // Try to start the session - this should trigger encoding
    bool started = fesk_session_start(&session);
    TEST_ASSERT_TRUE(started);

    // Check that encoded_payload was allocated (emojis should trigger encoding)
    TEST_ASSERT_NOT_NULL(session.encoded_payload);

    // Verify the encoded payload only contains valid fesk characters
    const char *payload = session.encoded_payload;
    size_t len = strlen(payload);
    TEST_ASSERT_GREATER_THAN(0, len);

    for (size_t i = 0; i < len; i++) {
        char ch = payload[i];
        // Valid fesk characters: a-z, 0-9, space, comma, colon, apostrophe, quote, newline
        bool is_valid = (ch >= 'a' && ch <= 'z') ||
                       (ch >= '0' && ch <= '9') ||
                       ch == ' ' || ch == ',' || ch == ':' ||
                       ch == '\'' || ch == '"' || ch == '\n';
        TEST_ASSERT_TRUE_MESSAGE(is_valid, "Encoded payload contains invalid character");
    }

    // Cleanup
    fesk_session_dispose(&session);
}

// Test various unicode characters
void test_session_unicode_encoding(void) {
    fesk_session_t session;
    fesk_session_config_t config = fesk_session_config_defaults();

    // Mix of valid and invalid characters
    config.static_message = "hello™ world€ 2025©";
    config.enable_countdown = false;

    fesk_session_init(&session, &config);
    bool started = fesk_session_start(&session);
    TEST_ASSERT_TRUE(started);

    // Should have been base32 encoded
    TEST_ASSERT_NOT_NULL(session.encoded_payload);

    // Verify all characters are valid
    const char *payload = session.encoded_payload;
    for (size_t i = 0; i < strlen(payload); i++) {
        uint8_t code;
        bool is_valid = fesk_lookup_char_code((unsigned char)payload[i], &code);
        TEST_ASSERT_TRUE_MESSAGE(is_valid, "Encoded character is not valid for fesk");
    }

    fesk_session_dispose(&session);
}

// Test that valid-only messages don't get encoded
void test_session_no_encoding_for_valid_chars(void) {
    fesk_session_t session;
    fesk_session_config_t config = fesk_session_config_defaults();

    config.static_message = "hello world 123";  // All valid characters
    config.enable_countdown = false;

    fesk_session_init(&session, &config);
    bool started = fesk_session_start(&session);
    TEST_ASSERT_TRUE(started);

    // Should NOT have been encoded (all chars were valid)
    TEST_ASSERT_NULL(session.encoded_payload);

    fesk_session_dispose(&session);
}

// Global flag for error callback test
static bool g_error_called = false;
static fesk_result_t g_last_error = FESK_OK;

// Error callback for testing
static void test_error_callback(fesk_result_t error, void *user_data) {
    (void)user_data;
    g_error_called = true;
    g_last_error = error;
}

// Test disabling auto encoding
void test_session_auto_encode_disabled(void) {
    fesk_session_t session;
    fesk_session_config_t config = fesk_session_config_defaults();

    g_error_called = false;
    g_last_error = FESK_OK;

    config.static_message = "hello™";  // Contains invalid character
    config.auto_base32_encode = false;  // Disable auto encoding
    config.enable_countdown = false;
    config.on_error = test_error_callback;

    fesk_session_init(&session, &config);
    bool started = fesk_session_start(&session);

    // Should fail to start because of invalid character
    TEST_ASSERT_FALSE(started);
    TEST_ASSERT_TRUE(g_error_called);
    TEST_ASSERT_EQUAL(FESK_ERR_UNSUPPORTED_CHARACTER, g_last_error);

    fesk_session_dispose(&session);
}

// Payload callback that returns emoji
static fesk_result_t test_emoji_payload_callback(const char **out_text, size_t *out_length, void *user_data) {
    (void)user_data;
    *out_text = "🚀🌟";  // Rocket and star emojis
    *out_length = 0;  // Let it calculate length
    return FESK_OK;
}

// Test payload callback with emojis
void test_session_callback_emoji_encoding(void) {
    fesk_session_t session;
    fesk_session_config_t config = fesk_session_config_defaults();

    config.provide_payload = test_emoji_payload_callback;
    config.enable_countdown = false;

    fesk_session_init(&session, &config);
    bool started = fesk_session_start(&session);
    TEST_ASSERT_TRUE(started);

    // Should have been base32 encoded
    TEST_ASSERT_NOT_NULL(session.encoded_payload);

    fesk_session_dispose(&session);
}

// Test that encoded payload is properly cleaned up
void test_session_encoded_payload_cleanup(void) {
    fesk_session_t session;
    fesk_session_config_t config = fesk_session_config_defaults();

    config.static_message = "emoji: 😀";
    config.enable_countdown = false;

    fesk_session_init(&session, &config);
    fesk_session_start(&session);

    TEST_ASSERT_NOT_NULL(session.encoded_payload);

    // Dispose should free the encoded payload
    fesk_session_dispose(&session);

    // After dispose, the pointer should be cleared (in theory, but we can't check after free)
    // This test mainly ensures no memory leaks occur
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_config_defaults_auto_base32_encode);
    RUN_TEST(test_session_emoji_encoding);
    RUN_TEST(test_session_unicode_encoding);
    RUN_TEST(test_session_no_encoding_for_valid_chars);
    RUN_TEST(test_session_auto_encode_disabled);
    RUN_TEST(test_session_callback_emoji_encoding);
    RUN_TEST(test_session_encoded_payload_cleanup);

    return UNITY_END();
}
