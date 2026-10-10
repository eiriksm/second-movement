#include "movement.h"
#include <time.h>

static bool movement_time_override_active = false;
static watch_date_time_t movement_time_override;

bool movement_default_loop_handler(movement_event_t event) {
    (void) event;
    return true;
}

static bool movement_moved_to_face;

void movement_move_to_face(uint8_t watch_face_index) {
    (void) watch_face_index;
    movement_moved_to_face = true;
}

void movement_move_to_next_face(void) {
}

watch_date_time_t movement_get_local_date_time(void) {
    if (movement_time_override_active) {
        return movement_time_override;
    }
    watch_date_time_t date_time = {0};
    // Set it to current time.
    time_t current_time;
    current_time = time(NULL);
    struct tm *time_info;
    time_info = gmtime(&current_time);
    date_time.unit.year = (time_info->tm_year + 1900) - 2020;
    date_time.unit.month = time_info->tm_mon + 1;
    date_time.unit.day = time_info->tm_mday;
    date_time.unit.hour = time_info->tm_hour;
    date_time.unit.minute = time_info->tm_min;
    date_time.unit.second = time_info->tm_sec;
    return date_time;
}

int32_t movement_get_current_timezone_offset_for_zone(uint8_t zone_index) {
    (void) zone_index;
    return 0;
}

int32_t movement_get_current_timezone_offset(void) {
    return 0;
}

void movement_request_tick_frequency(uint8_t freq) {
    (void)freq;
}

static uint32_t movement_utc_timestamp;
static int8_t *movement_last_sequence;
static uint8_t movement_sequence_count;
static bool movement_led_on;
static uint8_t movement_led_red;
static uint8_t movement_led_green;

void movement_set_utc_timestamp(uint32_t timestamp) {
    movement_utc_timestamp = timestamp;
}

void movement_play_sequence(int8_t *note_sequence, movement_buzzer_priority_t priority) {
    (void)priority;
    movement_last_sequence = note_sequence;
    movement_sequence_count++;
}

void movement_force_led_on(uint8_t red, uint8_t green, uint8_t blue) {
    (void)blue;
    movement_led_on = true;
    movement_led_red = red;
    movement_led_green = green;
}

void movement_force_led_off(void) {
    movement_led_on = false;
    movement_led_red = 0;
    movement_led_green = 0;
}

void movement_mock_reset(void) {
    movement_utc_timestamp = 0;
    movement_last_sequence = NULL;
    movement_sequence_count = 0;
    movement_moved_to_face = false;
    movement_force_led_off();
}

bool movement_mock_moved_to_face(void) {
    return movement_moved_to_face;
}

uint32_t movement_mock_get_utc_timestamp(void) {
    return movement_utc_timestamp;
}

int8_t *movement_mock_get_last_sequence(void) {
    return movement_last_sequence;
}

uint8_t movement_mock_get_sequence_count(void) {
    return movement_sequence_count;
}

bool movement_mock_led_is_on(void) {
    return movement_led_on;
}

uint8_t movement_mock_get_led_red(void) {
    return movement_led_red;
}

uint8_t movement_mock_get_led_green(void) {
    return movement_led_green;
}

void movement_mock_set_local_date_time(watch_date_time_t date_time) {
    movement_time_override = date_time;
    movement_time_override_active = true;
}

void movement_mock_clear_local_date_time(void) {
    movement_time_override_active = false;
}
