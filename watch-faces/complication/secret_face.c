
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "secret_face.h"
#include "watch_utility.h"
#include "zones.h"

static void draw(secret_state_t *state, uint8_t subsecond) {

}

void secret_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;

    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(secret_state_t));
        secret_state_t *state = (secret_state_t *)*context_ptr;
        memset(*context_ptr, 0, sizeof(secret_state_t));
        state->watch_face_index = watch_face_index;
    }
}

void secret_face_activate(void *context) {
    secret_state_t *state = (secret_state_t *)context;
    watch_set_colon();

    movement_request_tick_frequency(1);
}

bool secret_face_loop(movement_event_t event, void *context) {
    secret_state_t *state = (secret_state_t *)context;
    return true;
}

void secret_face_resign(void *context) {
    
}
