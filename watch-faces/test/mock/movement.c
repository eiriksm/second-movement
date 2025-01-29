#include "movement.h"

bool movement_default_loop_handler(movement_event_t event) {
    (void) event;
    return true;
}
void movement_move_to_face(uint8_t watch_face_index) {
    (void) watch_face_index;
}

watch_date_time_t movement_get_local_date_time(void) {
    return (watch_date_time_t){0};
}

int32_t movement_get_current_timezone_offset_for_zone(uint8_t zone_index) {
    (void) zone_index;
    return 0;
}

int32_t movement_get_current_timezone_offset(void) {
    return 0;
}
