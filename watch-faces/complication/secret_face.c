
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "secret_face.h"
#include "watch_utility.h"
#include "zones.h"

static int8_t round_win_melody[] = {
    BUZZER_NOTE_A4, 64,
    BUZZER_NOTE_B4, 32,
    BUZZER_NOTE_C5, 32,
    BUZZER_NOTE_D5, 32,
    BUZZER_NOTE_A4, 64,
    BUZZER_NOTE_REST, 32,
    BUZZER_NOTE_A4, 64,
    BUZZER_NOTE_B4, 32,
    BUZZER_NOTE_C5, 32,
    BUZZER_NOTE_D5, 64,
    BUZZER_NOTE_REST, 32,
    BUZZER_NOTE_A4, 64,
    BUZZER_NOTE_B4, 32,
    BUZZER_NOTE_C5, 32,
    BUZZER_NOTE_D5, 32,
    BUZZER_NOTE_A4, 64,
    BUZZER_NOTE_REST, 32,
    BUZZER_NOTE_E5, 64,
    BUZZER_NOTE_C5, 32,
    BUZZER_NOTE_A4, 32,
    BUZZER_NOTE_D5, 64,
    0};

static char* text[] = {
  "     L", "    L0", "   L0V", "  L0V ", " L0V U", "L0V U ",
  "0V U S", "V U SI", " U SIL", "U SILJ", " SILJE", "SILJE ",
  "ILJE H", "LJE HI", "JE HIL", "E HILS", " HILSE", "HILSEN",
  "ILSEN ", "LSEN M", "SEN MA", "EN MAR", "N MARJ", " MARJA",
  "MARJA,", "ARJA, ", "RJA, E", "JA, EI", "A, EIR", ", EIRI",
  " EIRIK", "EIRIK ", "IRIK O", "RIK OG", "IK OG ", "K OG L",
  " OG LA", "OG LAR", "G LARS", " LARS ", "LARS M", "ARS MO",
  "RS MON", "S MONS", " MONSE", "MONSEN", "ONSEN ", "NSEN  ",
  "SEN   ", "EN    ", "N     ", "      "
};

int array_size = sizeof(text) / sizeof(text[0]);

secret_state_t *my_secret;

static void draw(secret_state_t *state, uint8_t subsecond) {
    (void) subsecond;
    if (state->clicks == 3) {
      watch_buzzer_play_sequence(round_win_melody, NULL);
    }
    if (state->clicks < array_size) {
        watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, text[state->clicks], text[state->clicks]);
    }
}

void secret_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;

    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(secret_state_t));
        my_secret = (secret_state_t *)*context_ptr;
        memset(*context_ptr, 0, sizeof(secret_state_t));
        my_secret->watch_face_index = watch_face_index;
        my_secret->is_hidden = false;
    }
}

void secret_face_activate(void *context) {
    my_secret = (secret_state_t *)context;
    watch_set_colon();
}

bool secret_face_loop(movement_event_t event, void *context) {
    my_secret = (secret_state_t *)context;
    switch (event.event_type) {
        case EVENT_TICK:
            my_secret->clicks++;
            draw(my_secret, event.subsecond);
            break;
        case EVENT_ACTIVATE:
            if (my_secret->is_hidden) {
                movement_move_to_next_face();
                break;
            }
            my_secret->clicks = 0;
            draw(my_secret, event.subsecond);
            break;

        case EVENT_TIMEOUT:
            movement_move_to_face(0);
            break;

        default:
            movement_default_loop_handler(event);
            break;
    }

    return true;
}

void secret_face_resign(void *context) {

}

secret_state_t *get_secret_state() {
    return my_secret;
}

void set_secret_state(secret_state_t *state) {
    my_secret = state;
}
