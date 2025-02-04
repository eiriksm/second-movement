#ifndef SECRET_FACE_H_
#define SECRET_FACE_H_

#include "movement.h"

typedef struct {
    uint32_t target_ts;
    uint32_t now_ts;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t set_hours;
    uint8_t set_minutes;
    uint8_t set_seconds;
    uint8_t selection;
    uint8_t watch_face_index;
    uint8_t offset;
    uint8_t stopOffset;
} secret_state_t;


void secret_face_setup(uint8_t watch_face_index, void ** context_ptr);
void secret_face_activate(void *context);
bool secret_face_loop(movement_event_t event, void *context);
void secret_face_resign(void *context);

#define secret_face ((const watch_face_t){ \
    secret_face_setup, \
    secret_face_activate, \
    secret_face_loop, \
    secret_face_resign, \
    NULL, \
})

#endif // SECRET_FACE_H_
