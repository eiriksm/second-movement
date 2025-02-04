#ifndef SECRET_FACE_H_
#define SECRET_FACE_H_

#include "movement.h"

typedef struct {
    uint8_t watch_face_index;
    bool is_hidden;
} secret_state_t;


void secret_face_setup(uint8_t watch_face_index, void ** context_ptr);
void secret_face_activate(void *context);
bool secret_face_loop(movement_event_t event, void *context);
void secret_face_resign(void *context);

secret_state_t *get_secret_state();
void set_secret_state(secret_state_t *state);

#define secret_face ((const watch_face_t){ \
    secret_face_setup, \
    secret_face_activate, \
    secret_face_loop, \
    secret_face_resign, \
    NULL, \
})

#endif // SECRET_FACE_H_
