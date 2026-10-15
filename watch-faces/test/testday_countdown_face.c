#include "unity/src/unity.h"
#include "day_countdown_face.h"
#include "movement.h"
#include "watch.h"
#include "filesystem.h"

#include <stdlib.h>
#include <string.h>

static day_countdown_state_t *state;

static void set_date(int year, int month, int day) {
    watch_date_time_t date_time = {0};
    date_time.unit.year = year - 2020;
    date_time.unit.month = month;
    date_time.unit.day = day;
    date_time.unit.hour = 12;
    movement_mock_set_local_date_time(date_time);
}

static void write_days_file(const char *text) {
    filesystem_write_file("days.txt", (char *)text, strlen(text));
}

static void start(void) {
    void *context_ptr = NULL;
    day_countdown_face_setup(0, &context_ptr);
    state = (day_countdown_state_t *)context_ptr;
    day_countdown_face_activate(state);
    day_countdown_face_loop((movement_event_t){.event_type = EVENT_ACTIVATE}, state);
}

static void press(uint8_t event_type) {
    day_countdown_face_loop((movement_event_t){.event_type = event_type}, state);
}

static void assert_display(const char *label, const char *number, const char *bottom) {
    TEST_ASSERT_EQUAL_STRING(label, watch_get_display_text(WATCH_POSITION_TOP_LEFT));
    TEST_ASSERT_EQUAL_STRING(number, watch_get_display_text(WATCH_POSITION_TOP_RIGHT));
    TEST_ASSERT_EQUAL_STRING(bottom, watch_get_display_text(WATCH_POSITION_BOTTOM));
}

void setUp(void) {
    movement_mock_reset();
    filesystem_mock_reset();
    set_date(2026, 10, 15);
    state = NULL;
}

void tearDown(void) {
    if (state) {
        day_countdown_face_resign(state);
        free(state);
    }
    movement_mock_clear_local_date_time();
}

static void test_presets_without_file(void) {
    start();
    assert_display("NEW", " 1", "    78");
    press(EVENT_ALARM_BUTTON_UP);
    assert_display("EAS", " 2", "   164");
    press(EVENT_ALARM_BUTTON_UP);
    assert_display("17M", " 3", "   214");
    press(EVENT_ALARM_BUTTON_UP);
    assert_display("SUM", " 4", "   251");
    press(EVENT_ALARM_BUTTON_UP);
    assert_display("HAL", " 5", "    16");
    press(EVENT_ALARM_BUTTON_UP);
    assert_display("XMS", " 6", "    70");
    press(EVENT_ALARM_BUTTON_UP);
    assert_display("NEW", " 1", "    78");
}

static void test_file_counters_come_before_presets(void) {
    write_days_file("# comment\nVAC|1814400000\n\nbroken line\nOLD|1767225600\r\nVacation|1814400000");
    start();
    TEST_ASSERT_EQUAL(3, state->file_count);
    assert_display("VAC", " 1", "   259");
    press(EVENT_ALARM_BUTTON_UP);
    assert_display("OLD", " 2", "  -287");
    press(EVENT_ALARM_BUTTON_UP);
    // Long labels are cut short.
    assert_display("Vac", " 3", "   259");
    press(EVENT_ALARM_BUTTON_UP);
    assert_display("NEW", " 4", "    78");
}

static void test_long_press_shows_date(void) {
    write_days_file("VAC|1814400000\n");
    start();
    press(EVENT_ALARM_LONG_PRESS);
    assert_display("VAC", " 1", "270701");
    press(EVENT_ALARM_LONG_UP);
    assert_display("VAC", " 1", "   259");
}

static void test_preset_today_is_zero(void) {
    set_date(2027, 3, 28);
    start();
    press(EVENT_ALARM_BUTTON_UP);
    assert_display("EAS", " 2", "     0");
    // The day after, it counts down to next Easter, 16 April 2028.
    set_date(2027, 3, 29);
    press(EVENT_TICK);
    assert_display("EAS", " 2", "   384");
}

static void test_redraws_at_midnight(void) {
    start();
    assert_display("NEW", " 1", "    78");
    set_date(2026, 10, 16);
    press(EVENT_TICK);
    assert_display("NEW", " 1", "    77");
}

static void test_reloads_file_on_activate(void) {
    start();
    TEST_ASSERT_EQUAL(0, state->file_count);
    write_days_file("VAC|1814400000\n");
    day_countdown_face_resign(state);
    day_countdown_face_activate(state);
    press(EVENT_ACTIVATE);
    assert_display("VAC", " 1", "   259");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_presets_without_file);
    RUN_TEST(test_file_counters_come_before_presets);
    RUN_TEST(test_long_press_shows_date);
    RUN_TEST(test_preset_today_is_zero);
    RUN_TEST(test_redraws_at_midnight);
    RUN_TEST(test_reloads_file_on_activate);
    return UNITY_END();
}
