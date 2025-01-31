#ifndef UNIT_COUNTER_FACE_H_
#define UNIT_COUNTER_FACE_H_

#include <stdbool.h>
#include "movement.h"

typedef struct {
    struct {
        int volume;
        // Alcohol percentage. 4.7 would be stored as 47.
        int percentage;
    } units[20];
    uint8_t unit_count;
    uint32_t start_time; // Store start_time time of beer consumption
    uint32_t weight;
    uint8_t sex; // 0 for male, 1 for female
    uint32_t drink_vol;
    uint32_t alc_cont;
    uint8_t screen_delta;
    uint8_t edit_offset;
    bool edit_on;
    bool edit_weight;
    bool is_alc_cont_screen; //alcohol content of drink
} unit_counter_state_t;

void unit_counter_face_setup(uint8_t watch_face_index, void ** context_ptr);
void unit_counter_face_activate(void *context);
bool unit_counter_face_loop(movement_event_t event, void *context);
void unit_counter_face_resign(void *context);
void parse_bac_into_result(float val, char result[3][8]);
void print_edit_screen(unit_counter_state_t *state);
void unit_counter_print_settings_screen(unit_counter_state_t *state);

float unit_counter_calculate_bac(unit_counter_state_t *state);
static void print_unit_count(unit_counter_state_t *state);


static uint32_t calculate_time_to_sober(float current_bac);

#define unit_counter_face ((const watch_face_t){ \
    unit_counter_face_setup, \
    unit_counter_face_activate, \
    unit_counter_face_loop, \
    unit_counter_face_resign, \
    NULL, \
})

#endif // UNIT_COUNTER_FACE_H_
