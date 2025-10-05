/*
 * MIT License
 * (c) 2024 Second Movement Project
 */
#ifndef FESK_TX_H
#define FESK_TX_H

#include <stdint.h>
#include <stdbool.h>

/* Conditional include for watch library - only when building for watch */
#ifdef __SAML22J18A__
#include "watch_tcc.h"
#define FESK_HAS_WATCH_BUZZER 1
#else
#define FESK_HAS_WATCH_BUZZER 0
/* Define a placeholder buzzer note type for tests */
typedef int watch_buzzer_note_t;
#define BUZZER_NOTE_F7 0
#define BUZZER_NOTE_A7 1
#define BUZZER_NOTE_D8 2
#define BUZZER_NOTE_REST 3
#endif

/** Harmonic Triad 3-FSK (HT3) acoustic protocol:
 *  - 3-FSK using consonant 4:5:6 tone ratios
 *  - Binary preamble + Barker-13 sync on {f0,f2}
 *  - After sync: ternary symbols (0/1/2), MS-trit-first packing
 *  - Header (2 bytes length) + payload bytes are scrambled
 *  - CRC-16/CCITT over original payload; CRC bytes sent unscrumbled (big-endian)
 *  - Uses differential encoding for improved noise resilience
 */

/* -------- Protocol configuration -------- */
#define FESK_TONE_COUNT        3     /* 0..2 */
#define FESK_PREAMBLE_LEN      12
#define FESK_SYNC_LEN          13
#define FESK_HEADER_LEN        2     /* bytes: length field */
#define FESK_CRC_LEN           2     /* bytes */
#define FESK_SYMBOL_TICKS      6     /* 64Hz ticks per symbol (default) */
#define FESK_MAX_PAYLOAD_SIZE  256   /* payload bytes */

/* Safe upper bound for prebuilt trit stream.
 * Rule of thumb: trits ≈ ceil((payload+4) * 1.7).
 * 256B → ~437; use some headroom.
 */
#define FESK_MAX_TRITS         600

/* Default FESK frequencies mapped to closest buzzer notes */
#define FESK_F0 2794  /* F7 = 2793.83 Hz */
#define FESK_F1 3520  /* A7 = 3520.00 Hz */
#define FESK_F2 4699  /* D8 = 4698.63 Hz */

/* Buzzer note mappings */
#define FESK_NOTE_0 BUZZER_NOTE_F7
#define FESK_NOTE_1 BUZZER_NOTE_A7
#define FESK_NOTE_2 BUZZER_NOTE_D8

/* -------- Results -------- */
typedef enum {
    FESK_SUCCESS = 0,
    FESK_ERROR_INVALID_PARAM,
    FESK_ERROR_BUFFER_FULL,
    FESK_ERROR_PAYLOAD_TOO_LARGE,
    FESK_ERROR_NO_DATA
} fesk_result_t;

/* -------- Config -------- */
typedef struct {
    uint16_t f0, f1, f2;   /* Hz */
    uint8_t  symbol_ticks; /* 64Hz ticks per symbol */
    bool     use_scrambler;
    bool     use_fec;      /* reserved */
} fesk_config_t;

/* -------- Internal encoder state -------- */
typedef struct {
    /* Configuration */
    fesk_config_t       config;

    /* State machine */
    enum {
        FESK_STATE_PREAMBLE = 0,
        FESK_STATE_SYNC,
        FESK_STATE_PAYLOAD,  /* unified emission of header+payload+CRC trits */
        FESK_STATE_COMPLETE
    } state;

    /* Sequence counters for preamble/sync */
    uint16_t sequence_pos;

    /* Data buffers */
    uint8_t  payload_buffer[FESK_MAX_PAYLOAD_SIZE];
    uint16_t payload_len;

    /* Trit counting */
    uint32_t trit_count;    /* total data trits since sync (header+payload+CRC) */

    /* Differential encoding state */
    uint8_t  last_trit;     /* previous trit for differential encoding */

    /* CRC / Scrambler */
    uint16_t crc16;         /* CRC-16/CCITT over original payload */
    uint16_t lfsr_state;    /* x^9 + x^5 + 1, seed 0x1FF */

    /* Transmission status */
    bool     transmission_active;

    /* Tone periods for PWM (computed from frequencies) */
    uint16_t tone_periods[3];

    /* Canonical prebuilt trit stream (MS-trit-first) */
    uint8_t  trit_stream[FESK_MAX_TRITS];
    uint16_t trit_len;
    uint16_t trit_pos;
} fesk_encoder_state_t;

/* -------- Public API -------- */
fesk_result_t fesk_init_encoder(fesk_encoder_state_t *encoder,
                                const uint8_t *payload_data,
                                uint16_t payload_len);

fesk_result_t fesk_init_encoder_with_config(fesk_encoder_state_t *encoder,
                                            const fesk_config_t *config,
                                            const uint8_t *payload_data,
                                            uint16_t payload_len);

uint8_t  fesk_get_next_tone(fesk_encoder_state_t *encoder);
uint16_t fesk_get_tone_period(const fesk_encoder_state_t *encoder, uint8_t tone_index);
uint16_t fesk_get_tone_frequency(const fesk_encoder_state_t *encoder, uint8_t tone_index);
#if FESK_HAS_WATCH_BUZZER
watch_buzzer_note_t fesk_get_buzzer_note(uint8_t tone_index);
#endif
bool     fesk_is_transmitting(const fesk_encoder_state_t *encoder);
void     fesk_abort_transmission(fesk_encoder_state_t *encoder);

/* -------- Config helpers -------- */
void          fesk_get_default_config(fesk_config_t *config);
fesk_result_t fesk_validate_config(const fesk_config_t *config);

/* -------- Utilities -------- */
uint16_t fesk_crc16(const uint8_t *data, uint16_t length, uint16_t init_value);
uint16_t fesk_update_crc16(uint16_t crc, uint8_t byte_val);

#endif /* FESK_TX_H */
