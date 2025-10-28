#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fesk_tx.h"
#include "movement.h"

typedef fesk_result_t (*fesk_session_payload_cb)(const char **out_text,
                                                 size_t *out_length,
                                                 void *user_data);

typedef void (*fesk_session_simple_cb)(void *user_data);
typedef void (*fesk_session_error_cb)(fesk_result_t error, void *user_data);
typedef void (*fesk_session_countdown_cb)(uint8_t seconds_remaining, void *user_data);
typedef void (*fesk_session_sequence_cb)(const int8_t *sequence,
                                         size_t entries,
                                         void *user_data);

typedef struct {
    bool enable_countdown;
    uint8_t countdown_seconds;
    bool countdown_beep;
    bool show_bell_indicator;
    const char *static_message;
    size_t static_message_length;
    fesk_session_payload_cb provide_payload;
    fesk_session_simple_cb on_ready;
    fesk_session_simple_cb on_countdown_begin;
    fesk_session_countdown_cb on_countdown_tick;
    fesk_session_simple_cb on_countdown_complete;
    fesk_session_simple_cb on_transmission_start;
    fesk_session_sequence_cb on_sequence_ready;
    fesk_session_simple_cb on_transmission_end;
    fesk_session_simple_cb on_cancelled;
    fesk_session_error_cb on_error;
    void *user_data;
} fesk_session_config_t;

typedef enum {
    FESK_SESSION_IDLE = 0,
    FESK_SESSION_COUNTDOWN,
    FESK_SESSION_TRANSMITTING,
} fesk_session_phase_t;

typedef struct fesk_session_s {
    fesk_session_config_t config;
    fesk_session_phase_t phase;
    uint8_t seconds_remaining;
    int8_t *sequence;
    size_t sequence_entries;
} fesk_session_t;

fesk_session_config_t fesk_session_config_defaults(void);
void fesk_session_init(fesk_session_t *session, const fesk_session_config_t *config);
void fesk_session_dispose(fesk_session_t *session);
bool fesk_session_start(fesk_session_t *session);
void fesk_session_cancel(fesk_session_t *session);
void fesk_session_prepare(fesk_session_t *session);
bool fesk_session_is_idle(const fesk_session_t *session);
