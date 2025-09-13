/*
 * MIT License
 * (c) 2024 Second Movement Project
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "fesk_tx.h"

/* ---- Constants ---- */

/* 12-bit alternating preamble (binary, f0/f2) */
static const uint8_t preamble_pattern[FESK_PREAMBLE_LEN] = {
    1,0,1,0,1,0,1,0,1,0,1,0
};

/* Barker-13 sync sequence */
static const uint8_t barker13_pattern[FESK_SYNC_LEN] = {
    1,1,1,1,1,0,0,1,1,0,1,0,1
};

/* Scrambler polynomial x^9 + x^5 + 1 */
#define FESK_LFSR_SEED 0x1FF
#define FESK_CRC16_POLY 0x1021

/* ---- Internal helpers ---- */

/* Differential encoding state */
static uint8_t differential_encode_trit(fesk_encoder_state_t *e, uint8_t trit) {
    uint8_t encoded = (e->last_trit + trit) % 3;
    e->last_trit = encoded;
    return encoded;
}

/* Precompute tone periods for buzzer (1MHz clock) */
static void _fesk_init_periods(fesk_encoder_state_t *e) {
    e->tone_periods[0] = 1000000 / e->config.f0;
    e->tone_periods[1] = 1000000 / e->config.f1;
    e->tone_periods[2] = 1000000 / e->config.f2;
}

/* Reset LFSR state */
static void _fesk_init_lfsr(fesk_encoder_state_t *e) {
    e->lfsr_state = FESK_LFSR_SEED;
}

/* Scramble one byte with LFSR, advancing state (LSB-first XOR) */
static uint8_t _fesk_scramble_byte(fesk_encoder_state_t *e, uint8_t b) {
    if (!e->config.use_scrambler) return b;

    uint8_t out = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t lsb = e->lfsr_state & 1;
        uint8_t bit = ((b >> i) & 1) ^ lsb;
        out |= bit << i;

        uint8_t fb = ((e->lfsr_state >> 8) ^ (e->lfsr_state >> 4)) & 1;
        e->lfsr_state = ((e->lfsr_state << 1) | fb) & 0x1FF;
    }
    return out;
}

/* Pack bytes into trits, MS-trit-first, whole-number conversion */
static uint16_t pack_bytes_to_trits_msfirst(const uint8_t *bytes, uint16_t n, uint8_t *out) {
    uint8_t work[FESK_MAX_PAYLOAD_SIZE + 4];
    memcpy(work, bytes, n);
    uint16_t len = n;
    uint8_t tmp[FESK_MAX_TRITS];
    uint16_t tlen = 0;

    while (len > 0) {
        uint16_t newlen = 0, carry = 0;
        for (uint16_t i = 0; i < len; i++) {
            uint16_t cur = (carry << 8) | work[i];
            uint8_t q = (uint8_t)(cur / 3);
            carry = (uint8_t)(cur % 3);
            if (newlen || q) work[newlen++] = q;
        }
        tmp[tlen++] = carry; /* least-significant trit */
        len = newlen;
    }
    /* Reverse: emit MS-trit-first */
    for (uint16_t i = 0; i < tlen; i++) out[i] = tmp[tlen - 1 - i];
    return tlen;
}

/* Build entire ternary stream (header+payload+CRC) with differential encoding */
static void build_trit_stream(fesk_encoder_state_t *e) {
    if (e->config.use_scrambler) _fesk_init_lfsr(e);

    uint8_t bytes[FESK_MAX_PAYLOAD_SIZE + 4];
    uint16_t m = 0;

    /* Header (len, scrambled) */
    bytes[m++] = _fesk_scramble_byte(e, (e->payload_len >> 8) & 0xFF);
    bytes[m++] = _fesk_scramble_byte(e, e->payload_len & 0xFF);

    /* Payload (scrambled) */
    for (uint16_t i = 0; i < e->payload_len; i++)
        bytes[m++] = _fesk_scramble_byte(e, e->payload_buffer[i]);

    /* CRC over original payload, UNSCRAMBLED big-endian */
    bytes[m++] = (uint8_t)(e->crc16 >> 8);
    bytes[m++] = (uint8_t)(e->crc16 & 0xFF);

    printf("TX PACKED BYTES:");
    for (uint16_t i = 0; i < m; i++) printf(" %02X", bytes[i]);
    printf("\n");

    /* Convert to trits and apply differential encoding */
    uint8_t raw_trits[FESK_MAX_TRITS];
    uint16_t raw_trit_len = pack_bytes_to_trits_msfirst(bytes, m, raw_trits);
    
    /* Apply differential encoding to each trit */
    e->last_trit = 0;  /* Initialize differential state to 0 */
    for (uint16_t i = 0; i < raw_trit_len; i++) {
        e->trit_stream[i] = differential_encode_trit(e, raw_trits[i]);
    }
    
    e->trit_len = raw_trit_len;
    e->trit_pos = 0;
}

/* ---- CRC ---- */

uint16_t fesk_update_crc16(uint16_t crc, uint8_t b) {
    crc ^= (uint16_t)b << 8;
    for (int i = 0; i < 8; i++)
        crc = (crc & 0x8000) ? (crc << 1) ^ FESK_CRC16_POLY : (crc << 1);
    return crc;
}

uint16_t fesk_crc16(const uint8_t *data, uint16_t len, uint16_t init) {
    uint16_t crc = init;
    for (uint16_t i = 0; i < len; i++) crc = fesk_update_crc16(crc, data[i]);
    return crc;
}

/* ---- Public API ---- */

void fesk_get_default_config(fesk_config_t *c) {
    if (!c) return;
    c->f0 = FESK_F0; c->f1 = FESK_F1; c->f2 = FESK_F2;
    c->symbol_ticks = FESK_SYMBOL_TICKS;
    c->use_scrambler = true;
    c->use_fec = false;
}

fesk_result_t fesk_validate_config(const fesk_config_t *c) {
    if (!c) return FESK_ERROR_INVALID_PARAM;
    if (!c->f0 || !c->f1 || !c->f2) return FESK_ERROR_INVALID_PARAM;
    if (!c->symbol_ticks || c->symbol_ticks > 16) return FESK_ERROR_INVALID_PARAM;
    return FESK_SUCCESS;
}

fesk_result_t fesk_init_encoder(fesk_encoder_state_t *e,
                                const uint8_t *data,
                                uint16_t len) {
    fesk_config_t cfg;
    fesk_get_default_config(&cfg);
    return fesk_init_encoder_with_config(e, &cfg, data, len);
}

fesk_result_t fesk_init_encoder_with_config(fesk_encoder_state_t *e,
                                            const fesk_config_t *cfg,
                                            const uint8_t *data,
                                            uint16_t len) {
    if (!e || !cfg || !data) return FESK_ERROR_INVALID_PARAM;
    if (len > FESK_MAX_PAYLOAD_SIZE) return FESK_ERROR_PAYLOAD_TOO_LARGE;
    if (fesk_validate_config(cfg) != FESK_SUCCESS) return FESK_ERROR_INVALID_PARAM;

    memset(e, 0, sizeof(*e));
    e->config = *cfg;

    _fesk_init_periods(e);
    memcpy(e->payload_buffer, data, len);
    e->payload_len = len;

    e->crc16 = fesk_crc16(data, len, 0xFFFF);
    e->state = FESK_STATE_PREAMBLE;
    e->transmission_active = true;
    e->sequence_pos = 0;
    e->bit_pos = 0;
    e->trit_count = 0;
    e->last_trit = 0;
    return FESK_SUCCESS;
}

/* Get next tone: 0=f0, 1=f1, 2=f2; 255=end */
uint8_t fesk_get_next_tone(fesk_encoder_state_t *e) {
    if (!e || !e->transmission_active) return 255;

    switch (e->state) {
    case FESK_STATE_PREAMBLE:
        if (e->sequence_pos < FESK_PREAMBLE_LEN) {
            uint8_t bit = preamble_pattern[e->sequence_pos++];
            return bit ? 2 : 0;
        }
        e->state = FESK_STATE_SYNC;
        e->sequence_pos = 0;
        /* fallthrough */

    case FESK_STATE_SYNC:
        if (e->sequence_pos < FESK_SYNC_LEN) {
            uint8_t bit = barker13_pattern[e->sequence_pos++];
            return bit ? 2 : 0;
        }
        build_trit_stream(e);
        e->state = FESK_STATE_PAYLOAD;
        /* fallthrough */

    case FESK_STATE_HEADER: /* not used anymore */
    case FESK_STATE_PAYLOAD:
        /* Direct trit transmission - no pilot insertion */
        if (e->trit_pos < e->trit_len) {
            e->trit_count++;
            return e->trit_stream[e->trit_pos++];
        }
        e->state = FESK_STATE_COMPLETE;
        e->transmission_active = false;
        return 255;

    case FESK_STATE_CRC: /* obsolete path */
    case FESK_STATE_COMPLETE:
    default:
        e->transmission_active = false;
        return 255;
    }
}

/* Tone helpers */
uint16_t fesk_get_tone_period(const fesk_encoder_state_t *e, uint8_t idx) {
    if (!e || idx >= FESK_TONE_COUNT) return 0;
    return e->tone_periods[idx];
}
uint16_t fesk_get_tone_frequency(const fesk_encoder_state_t *e, uint8_t idx) {
    if (!e || idx >= FESK_TONE_COUNT) return 0;
    switch (idx) { case 0: return e->config.f0; case 1: return e->config.f1; case 2: return e->config.f2; }
    return 0;
}

/* Transmission status */
bool fesk_is_transmitting(const fesk_encoder_state_t *e) {
    return e && e->transmission_active;
}
void fesk_abort_transmission(fesk_encoder_state_t *e) {
    if (!e) return;
    e->transmission_active = false;
    e->state = FESK_STATE_COMPLETE;
}
