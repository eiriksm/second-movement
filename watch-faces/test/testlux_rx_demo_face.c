#include "unity/src/unity.h"
#include "lux_rx_demo_face.h"
#include "movement.h"
#include "watch.h"
#include "adc.h"
#include "filesystem.h"
#include "base32.h"

#include <stdlib.h>
#include <string.h>

#define BRIGHT 0
#define DARK 0xFFFF

static lux_rx_demo_context_t *ctx;

void setUp(void) {
    movement_mock_reset();
    filesystem_mock_reset();
    adc_mock_set_value(DARK);
    watch_display_text(WATCH_POSITION_TOP, "");
    watch_display_text(WATCH_POSITION_BOTTOM, "");

    void *context_ptr = NULL;
    lux_rx_demo_face_setup(0, &context_ptr);
    ctx = (lux_rx_demo_context_t *)context_ptr;
    lux_rx_demo_face_activate(ctx);
    lux_rx_demo_face_loop((movement_event_t){.event_type = EVENT_ACTIVATE}, ctx);
}

void tearDown(void) {
    lux_rx_demo_face_resign(ctx);
    free(ctx);
}

static void tick(uint16_t adc_value) {
    adc_mock_set_value(adc_value);
    lux_rx_demo_face_loop((movement_event_t){.event_type = EVENT_TICK}, ctx);
}

// Sends text to the face as light, one sample per tick, the way a
// transmitter would.
static void send_text(const char *text) {
    lux_rx_encoder_t enc;
    uint8_t bit;
    tick(DARK);
    lux_rx_encode(&enc, text);
    while (lux_rx_encode_next(&enc, &bit)) {
        tick(bit ? BRIGHT : DARK);
    }
}

static void send_base32(const char *text) {
    unsigned char coded[BASE32_LEN(LUX_RX_MAX_PAYLOAD) + 1] = {0};
    base32_encode((const unsigned char *)text, strlen(text), coded);
    send_text((const char *)coded);
}

static void assert_display(const char *top, const char *bottom) {
    TEST_ASSERT_EQUAL_STRING(top, watch_get_display_text(WATCH_POSITION_TOP));
    TEST_ASSERT_EQUAL_STRING(bottom, watch_get_display_text(WATCH_POSITION_BOTTOM));
}

static void assert_led_green(void) {
    TEST_ASSERT_TRUE(movement_mock_led_is_on());
    TEST_ASSERT_EQUAL(0, movement_mock_get_led_red());
    TEST_ASSERT_EQUAL(48, movement_mock_get_led_green());
}

static void assert_led_red(void) {
    TEST_ASSERT_TRUE(movement_mock_led_is_on());
    TEST_ASSERT_EQUAL(48, movement_mock_get_led_red());
    TEST_ASSERT_EQUAL(0, movement_mock_get_led_green());
}

// The sounds are told apart by their first note.
static void assert_last_sound_starts_with(int8_t note) {
    TEST_ASSERT_NOT_NULL(movement_mock_get_last_sequence());
    TEST_ASSERT_EQUAL(note, movement_mock_get_last_sequence()[0]);
}

static void test_activate_shows_waiting(void) {
    assert_display("LUX r", "WAIT  ");
}

static void test_receives_plain_text(void) {
    send_text("hello world");
    assert_display("RECV ", " hello");
    TEST_ASSERT_EQUAL_STRING("hello world", ctx->rx.payload);
    assert_led_green();
    assert_last_sound_starts_with(BUZZER_NOTE_C5);
}

static void test_plays_sound_once_while_done(void) {
    send_text("hi");
    tick(DARK);
    tick(DARK);
    TEST_ASSERT_EQUAL(1, movement_mock_get_sequence_count());
}

static void test_timestamp_sets_time(void) {
    send_text("1700000000");
    assert_display("RECV ", "SET   ");
    // The frame takes 624 ticks at 64 Hz, so 9 seconds are added.
    TEST_ASSERT_EQUAL_UINT32(1700000009, movement_mock_get_utc_timestamp());
    assert_last_sound_starts_with(BUZZER_NOTE_E6);
}

static void test_echo_transmits_text(void) {
    send_text("echo hi");
    assert_display("ECHO ", " hi   ");
    TEST_ASSERT_EQUAL(2, ctx->tx_buf_len);
    TEST_ASSERT_EQUAL_MEMORY("hi", ctx->tx_buf, 2);
}

static void test_echo_redirect_writes_file(void) {
    send_text("echo hello > a.txt");
    assert_display("ECHO ", "SAVED ");
    TEST_ASSERT_EQUAL_STRING("hello\n", filesystem_mock_get_file("a.txt"));
    assert_led_green();
    assert_last_sound_starts_with(BUZZER_NOTE_C5);
}

static void test_echo_redirect_appends_file(void) {
    filesystem_write_file("a.txt", "one\n", 4);
    send_text("echo two >> a.txt");
    assert_display("ECHO ", "SAVED ");
    TEST_ASSERT_EQUAL_STRING("one\ntwo\n", filesystem_mock_get_file("a.txt"));
}

static void test_echo_redirect_rejects_bad_filename(void) {
    send_text("echo hello > a/b");
    assert_display("ECHO ", "FAIL  ");
    TEST_ASSERT_NULL(filesystem_mock_get_file("a/b"));
    assert_led_red();
    assert_last_sound_starts_with(BUZZER_NOTE_E5);
}

static void test_ls_lists_files(void) {
    filesystem_write_file("a.txt", "abc", 3);
    filesystem_write_file("b", "", 0);
    send_text("ls");
    assert_display("LS   ", "LIST  ");
    const char expected[] = {'f', 3, 0, 'a', '.', 't', 'x', 't', '\0', 'f', 0, 0, 'b', '\0'};
    TEST_ASSERT_EQUAL(sizeof(expected), ctx->tx_buf_len);
    TEST_ASSERT_EQUAL_MEMORY(expected, ctx->tx_buf, sizeof(expected));
}

static void test_base32_is_decoded(void) {
    send_base32("Hello/World$");
    TEST_ASSERT_EQUAL_STRING("Hello/World$", ctx->rx.payload);
    assert_display("RECV ", " Hello");
}

static void test_base32_command_is_run(void) {
    // Uppercase letters are outside the lux_rx alphabet, so this only works
    // through base32.
    send_base32("echo Hi There > x.txt");
    assert_display("ECHO ", "SAVED ");
    TEST_ASSERT_EQUAL_STRING("Hi There\n", filesystem_mock_get_file("x.txt"));
}

static void test_base32_is_decoded_once(void) {
    // "MZXW6===" is the base32 of "foo", so the decoded text would itself
    // decode again if the face decoded on every tick.
    send_base32("MZXW6===");
    tick(DARK);
    TEST_ASSERT_EQUAL_STRING("MZXW6===", ctx->rx.payload);
}

static void test_long_base32_is_decoded(void) {
    // The largest decoded payload: LUX_RX_MAX_PAYLOAD base32 symbols.
    static char text[UNBASE32_LEN(LUX_RX_MAX_PAYLOAD) + 1];
    size_t len = sizeof(text) - 1;
    for (size_t i = 0; i < len; i++) text[i] = "Hello World $|{}"[i % 16];
    text[len] = '\0';
    send_base32(text);
    TEST_ASSERT_EQUAL(len, ctx->rx.payload_len);
    TEST_ASSERT_EQUAL_STRING(text, ctx->rx.payload);
    assert_display("RECV ", " Hello");
}

static void test_base32_utf8_is_decoded(void) {
    send_base32("\xc3\xa9t\xc3\xa9\ttab\r\n");
    TEST_ASSERT_EQUAL_STRING("\xc3\xa9t\xc3\xa9\ttab\r\n", ctx->rx.payload);
    // Bytes the display has no glyph for show as spaces.
    assert_display("RECV ", "   t  ");
}

static void test_base32_binary_is_not_decoded(void) {
    // Decodes to 0x00 0x01 0xff, which is not text.
    send_text("aaa76===");
    TEST_ASSERT_EQUAL_STRING("aaa76===", ctx->rx.payload);
}

static void test_timeout_does_not_leave_face(void) {
    lux_rx_demo_face_loop((movement_event_t){.event_type = EVENT_TIMEOUT}, ctx);
    TEST_ASSERT_FALSE(movement_mock_moved_to_face());
    send_text("still here");
    TEST_ASSERT_EQUAL_STRING("still here", ctx->rx.payload);
}

static void test_plain_word_is_not_base32(void) {
    // Valid base32 that decodes to printable text, but has no digit or padding.
    send_text("password");
    TEST_ASSERT_EQUAL_STRING("password", ctx->rx.payload);
    assert_display("RECV ", " passw");
}

static void test_bad_crc_shows_error(void) {
    lux_rx_encoder_t enc;
    uint8_t bit;
    tick(DARK);
    lux_rx_encode(&enc, "hi");
    // Flip the first bit of the CRC symbol, after START and two data symbols.
    uint16_t crc_start = (6 + 2 * 6) * LUX_RX_SAMPLES_PER_BIT;
    for (uint16_t i = 0; lux_rx_encode_next(&enc, &bit); i++) {
        if (i >= crc_start && i < crc_start + LUX_RX_SAMPLES_PER_BIT) bit = !bit;
        tick(bit ? BRIGHT : DARK);
    }
    assert_display("ERR  ", "FAIL  ");
    assert_led_red();
    assert_last_sound_starts_with(BUZZER_NOTE_E5);
}

static void test_alarm_button_resets_after_receive(void) {
    send_text("hi");
    lux_rx_demo_face_loop((movement_event_t){.event_type = EVENT_ALARM_BUTTON_UP}, ctx);
    assert_display("LUX r", "WAIT  ");
    TEST_ASSERT_FALSE(movement_mock_led_is_on());

    send_text("again");
    assert_display("RECV ", " again");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_activate_shows_waiting);
    RUN_TEST(test_receives_plain_text);
    RUN_TEST(test_plays_sound_once_while_done);
    RUN_TEST(test_timestamp_sets_time);
    RUN_TEST(test_echo_transmits_text);
    RUN_TEST(test_echo_redirect_writes_file);
    RUN_TEST(test_echo_redirect_appends_file);
    RUN_TEST(test_echo_redirect_rejects_bad_filename);
    RUN_TEST(test_ls_lists_files);
    RUN_TEST(test_base32_is_decoded);
    RUN_TEST(test_base32_command_is_run);
    RUN_TEST(test_base32_is_decoded_once);
    RUN_TEST(test_long_base32_is_decoded);
    RUN_TEST(test_base32_utf8_is_decoded);
    RUN_TEST(test_base32_binary_is_not_decoded);
    RUN_TEST(test_timeout_does_not_leave_face);
    RUN_TEST(test_plain_word_is_not_base32);
    RUN_TEST(test_bad_crc_shows_error);
    RUN_TEST(test_alarm_button_resets_after_receive);
    return UNITY_END();
}
