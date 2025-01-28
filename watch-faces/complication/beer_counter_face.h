#ifndef BEER_COUNTER_FACE_H_
#define BEER_COUNTER_FACE_H_

#include <stdbool.h>
#include "movement.h"

typedef struct {
    struct {
        int volume;
        // Alcohol percentage. 4.7 would be stored as 47.
        int percentage;
    } units[20];
    uint8_t beer_count;
    uint32_t start_time; // Store start_time time of beer consumption
    uint32_t weight;
    uint8_t sex; // 0 for male, 1 for female
    uint32_t drink_vol;
    uint32_t alc_cont;
    uint8_t screen_delta;
    uint8_t edit_offset;
    bool edit_on;
    bool is_alc_cont_screen; //alcohol content of drink
} beer_counter_state_t;

void beer_counter_face_setup(uint8_t watch_face_index, void ** context_ptr);
void beer_counter_face_activate(void *context);
bool beer_counter_face_loop(movement_event_t event, void *context);
void beer_counter_face_resign(void *context);
void parse_bac_into_result(float val, char result[3][8]);
void print_edit_screen(beer_counter_state_t *state);

static float calculate_bac(beer_counter_state_t *state);
static void print_unit_count(beer_counter_state_t *state);


static uint32_t calculate_time_to_sober(float current_bac);

#define beer_counter_face ((const watch_face_t){ \
    beer_counter_face_setup, \
    beer_counter_face_activate, \
    beer_counter_face_loop, \
    beer_counter_face_resign, \
    NULL, \
})

#endif // BEER_COUNTER_FACE_H_
