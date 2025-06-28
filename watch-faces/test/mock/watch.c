#include "watch.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

void watch_set_led_off(void) {}

// Create a struct to hold the current display text. It should be indexed by the different positions.
static char display_text[2][8];

void watch_display_text(watch_position_t location, const char *string) {
    // Copy the string into the display_text array.
    // So convert the location to an index.
    int delta = 0;
    switch (location) {
        case WATCH_POSITION_FULL:
            delta = 0;
            break;
        case WATCH_POSITION_BOTTOM:
            delta = 1;
            break;

    }
    // Now place that string into the display text array, at the given location.
    strcpy(display_text[delta], string);
}

char *watch_get_display_text(watch_position_t location) {
    // Convert the location to an index.
    int delta = 0;
    switch (location) {
        case WATCH_POSITION_FULL:
            delta = 0;
            break;
        case WATCH_POSITION_BOTTOM:
            delta = 1;
            break;
    }
    // Now return the string at that location.
    return display_text[delta];
}

void watch_display_text_with_fallback(watch_position_t location, const char *string, const char *fallback) {}

uint32_t watch_utility_date_time_to_unix_time(watch_date_time_t date_time, int32_t utc_offset) {
    // Convert from watch_date_time_t to tm.
    struct tm time_info;
    time_info.tm_sec = date_time.unit.second;
    time_info.tm_min = date_time.unit.minute;
    time_info.tm_hour = date_time.unit.hour;
    time_info.tm_mday = date_time.unit.day;
    time_info.tm_mon = date_time.unit.month - 1;
    time_info.tm_year = (date_time.unit.year + 2020) - 1900;
    // This one should be in the UTC timezone.
    time_info.tm_isdst = 0;
    // Now convert to unix time.
    time_t unix_time = mktime(&time_info);
    return unix_time - (utc_offset * 3600);
}
