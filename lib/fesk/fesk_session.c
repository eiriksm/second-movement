#include "fesk_session.h"

#include <string.h>

#include "movement.h"
#include "watch.h"
#include "watch_tcc.h"

#define FESK_SESSION_DEFAULT_COUNTDOWN_SECONDS 3
#define FESK_SESSION_TICKS_PER_SECOND 64
#define FESK_COUNTDOWN_BEEP_TICKS 8

static fesk_session_t *_fesk_active_session = NULL;

static const int8_t _fesk_countdown_sequence[] = {
    BUZZER_NOTE_A5, FESK_COUNTDOWN_BEEP_TICKS,
    BUZZER_NOTE_REST, (FESK_SESSION_TICKS_PER_SECOND - FESK_COUNTDOWN_BEEP_TICKS),
    0
};

static const int8_t _fesk_countdown_silence_sequence[] = {
    BUZZER_NOTE_REST, FESK_SESSION_TICKS_PER_SECOND,
    0
};

static inline uint8_t _get_countdown_seconds(const fesk_session_config_t *config) {
    uint8_t value = config->countdown_seconds;
    if (value == 0) {
        value = FESK_SESSION_DEFAULT_COUNTDOWN_SECONDS;
    }
    return value;
}

static void _call_simple(fesk_session_simple_cb cb, void *user_data) {
    if (cb) {
        cb(user_data);
    }
}

static void _call_countdown(fesk_session_countdown_cb cb,
                            uint8_t seconds,
                            void *user_data) {
    if (cb) {
        cb(seconds, user_data);
    }
}

static void _call_sequence(fesk_session_sequence_cb cb,
                           const int8_t *sequence,
                           size_t entries,
                           void *user_data) {
    if (cb) {
        cb(sequence, entries, user_data);
    }
}

static void _call_error(fesk_session_error_cb cb,
                        fesk_result_t error,
                        void *user_data) {
    if (cb) {
        cb(error, user_data);
    }
}

static void _clear_sequence(fesk_session_t *session) {
    if (session->sequence) {
        fesk_free_sequence(session->sequence);
        session->sequence = NULL;
        session->sequence_entries = 0;
    }
}

static void _finish_session(fesk_session_t *session, bool notify) {
    if (session->config.show_bell_indicator) {
        watch_clear_indicator(WATCH_INDICATOR_BELL);
    }

    if (_fesk_active_session == session) {
        _fesk_active_session = NULL;
    }

    watch_buzzer_abort_sequence();
    watch_set_buzzer_off();

    _clear_sequence(session);
    session->phase = FESK_SESSION_IDLE;
    session->seconds_remaining = 0;

    if (notify) {
        _call_simple(session->config.on_transmission_end, session->config.user_data);
    }
}

static bool _build_sequence(fesk_session_t *session) {
    const char *payload = session->config.static_message;
    size_t payload_length = session->config.static_message_length;

    if (session->config.provide_payload) {
        fesk_result_t callback_result = session->config.provide_payload(&payload,
                                                                        &payload_length,
                                                                        session->config.user_data);
        if (callback_result != FESK_OK) {
            _call_error(session->config.on_error, callback_result, session->config.user_data);
            return false;
        }
    }

    if (payload && payload_length == 0) {
        payload_length = strlen(payload);
    }

    if (!payload || payload_length == 0) {
        _call_error(session->config.on_error, FESK_ERR_INVALID_ARGUMENT, session->config.user_data);
        return false;
    }

    int8_t *sequence = NULL;
    size_t entries = 0;
    fesk_result_t encode_result = fesk_encode_text(payload,
                                                   payload_length,
                                                   &sequence,
                                                   &entries);
    if (encode_result != FESK_OK) {
        _call_error(session->config.on_error, encode_result, session->config.user_data);
        return false;
    }

    _clear_sequence(session);
    session->sequence = sequence;
    session->sequence_entries = entries;
    _call_sequence(session->config.on_sequence_ready,
                   sequence,
                   entries,
                   session->config.user_data);
    return true;
}

static void _fesk_transmission_complete(void);
static void _fesk_countdown_step_done(void);

static bool _start_transmission(fesk_session_t *session) {
    if (!_build_sequence(session)) {
        _finish_session(session, false);
        return false;
    }

    session->phase = FESK_SESSION_TRANSMITTING;

    if (session->config.show_bell_indicator) {
        watch_set_indicator(WATCH_INDICATOR_BELL);
    }

    if (_fesk_active_session && _fesk_active_session != session) {
        _finish_session(_fesk_active_session, false);
    }
    _fesk_active_session = session;

    _call_simple(session->config.on_transmission_start, session->config.user_data);
    watch_buzzer_play_sequence(session->sequence, _fesk_transmission_complete);
    return true;
}

static void _start_countdown(fesk_session_t *session) {
    session->phase = FESK_SESSION_COUNTDOWN;
    session->seconds_remaining = _get_countdown_seconds(&session->config);

    if (session->config.show_bell_indicator) {
        watch_set_indicator(WATCH_INDICATOR_BELL);
    }

    _call_simple(session->config.on_countdown_begin, session->config.user_data);
    _call_countdown(session->config.on_countdown_tick,
                    session->seconds_remaining,
                    session->config.user_data);

    if (_fesk_active_session && _fesk_active_session != session) {
        _finish_session(_fesk_active_session, false);
    }

    _fesk_active_session = session;

    const int8_t *sequence = session->config.countdown_beep
                               ? _fesk_countdown_sequence
                               : _fesk_countdown_silence_sequence;
    watch_buzzer_play_sequence((int8_t *)sequence, _fesk_countdown_step_done);
}

static bool _handle_start_request(fesk_session_t *session) {
    if (session->phase != FESK_SESSION_IDLE) {
        return false;
    }

    if (session->config.enable_countdown) {
        _start_countdown(session);
    } else {
        return _start_transmission(session);
    }
    return true;
}

static bool _handle_cancel(fesk_session_t *session) {
    if (session->phase == FESK_SESSION_IDLE) {
        return false;
    }

    _finish_session(session, false);
    _call_simple(session->config.on_cancelled, session->config.user_data);
    return true;
}

static void _fesk_transmission_complete(void) {
    if (!_fesk_active_session) {
        return;
    }
    fesk_session_t *session = _fesk_active_session;
    _fesk_active_session = NULL;
    _finish_session(session, true);
}

static void _fesk_countdown_step_done(void) {
    if (!_fesk_active_session) {
        return;
    }
    fesk_session_t *session = _fesk_active_session;
    if (session->phase != FESK_SESSION_COUNTDOWN) {
        return;
    }

    if (session->seconds_remaining > 0) {
        session->seconds_remaining--;
    }

    if (session->seconds_remaining == 0) {
        _call_countdown(session->config.on_countdown_tick,
                        session->seconds_remaining,
                        session->config.user_data);
        _call_simple(session->config.on_countdown_complete, session->config.user_data);
        _start_transmission(session);
        return;
    }

    _call_countdown(session->config.on_countdown_tick,
                    session->seconds_remaining,
                    session->config.user_data);

    const int8_t *sequence = session->config.countdown_beep
                               ? _fesk_countdown_sequence
                               : _fesk_countdown_silence_sequence;
    watch_buzzer_play_sequence((int8_t *)sequence, _fesk_countdown_step_done);
}

void fesk_session_init(fesk_session_t *session, const fesk_session_config_t *config) {
    if (!session) {
        return;
    }
    memset(session, 0, sizeof(*session));
    if (config) {
        session->config = *config;
    }

    if (session->config.countdown_seconds == 0 && session->config.enable_countdown) {
        session->config.countdown_seconds = FESK_SESSION_DEFAULT_COUNTDOWN_SECONDS;
    }

    session->phase = FESK_SESSION_IDLE;
    session->sequence = NULL;
    session->sequence_entries = 0;
}

void fesk_session_dispose(fesk_session_t *session) {
    if (!session) {
        return;
    }
    if (_fesk_active_session == session) {
        _fesk_active_session = NULL;
        watch_buzzer_abort_sequence();
    }
    _finish_session(session, false);
}

bool fesk_session_start(fesk_session_t *session) {
    if (!session) {
        return false;
    }
    return _handle_start_request(session);
}

void fesk_session_cancel(fesk_session_t *session) {
    if (!session) {
        return;
    }
    _handle_cancel(session);
}

void fesk_session_prepare(fesk_session_t *session) {
    if (!session) {
        return;
    }
    session->phase = FESK_SESSION_IDLE;
    session->seconds_remaining = 0;
    _call_simple(session->config.on_ready, session->config.user_data);
}

bool fesk_session_is_idle(const fesk_session_t *session) {
    if (!session) {
        return true;
    }
    return session->phase == FESK_SESSION_IDLE;
}
