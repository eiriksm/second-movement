#include "watch.h"

void watch_set_led_off(void) {}
void watch_display_text(watch_position_t location, const char *string) {}

void watch_display_text_with_fallback(watch_position_t location, const char *string, const char *fallback) {}

uint32_t watch_utility_date_time_to_unix_time(watch_date_time_t date_time, int32_t utc_offset) {
    (void) date_time;
    (void) utc_offset;
    return 0;
}
