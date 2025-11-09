/*
    Bits cribbed from Alex Taradov's BSD-3 licensed UART peripheral,
    Copyright (c) 2017-2023, Alex Taradov <alex@taradov.com>. All rights reserved.
    Full text available at: https://opensource.org/licenses/BSD-3
    Other stuff is MIT-licensed by me, Joey Castillo.
    Copyright 2023 Joey Castillo for Oddly Specific Objects.
    Published under the standard MIT License.
    Full text available at: https://opensource.org/licenses/MIT
*/

#include "pins.h"
#include "system.h"
#include "sercom.h"
#include "uart.h"
#include <string.h>

// Simulator UART buffer implementation
#define MAX_SERCOM_INSTANCES 8
#define UART_BUFFER_SIZE 512

typedef struct {
    char rx_buffer[UART_BUFFER_SIZE];
    size_t rx_read_pos;
    size_t rx_write_pos;
    size_t rx_count;
    bool enabled;
    bool irda_mode;
    uint32_t baud;
} uart_sim_state_t;

static uart_sim_state_t uart_instances[MAX_SERCOM_INSTANCES] = {0};

// Helper function to add data to UART buffer (for testing/UI integration)
// This function can be called from JavaScript via Emscripten or from test code
void uart_sim_inject_data(uint8_t sercom, const char *data, size_t length) {
    if (sercom >= MAX_SERCOM_INSTANCES) return;

    uart_sim_state_t *state = &uart_instances[sercom];

    for (size_t i = 0; i < length; i++) {
        if (state->rx_count < UART_BUFFER_SIZE) {
            state->rx_buffer[state->rx_write_pos] = data[i];
            state->rx_write_pos = (state->rx_write_pos + 1) % UART_BUFFER_SIZE;
            state->rx_count++;
        }
    }
}

// Helper function to check buffer status
size_t uart_sim_get_buffer_count(uint8_t sercom) {
    if (sercom >= MAX_SERCOM_INSTANCES) return 0;
    return uart_instances[sercom].rx_count;
}

#if defined(UART_SERCOM)

bool uart_read_byte(char *byte) {
    return uart_read_byte_instance(UART_SERCOM, byte);
}

void uart_init(uint32_t baud) {
    uart_init_instance(UART_SERCOM, UART_TXPO_0, UART_RXPO_0, baud);
}

void uart_set_run_in_standby(bool run_in_standby) {
    uart_set_run_in_standby_instance(UART_SERCOM, run_in_standby);
}

void uart_set_irda_mode(bool irda) {
    uart_set_irda_mode_instance(UART_SERCOM, irda);
}

void uart_enable(void) {
    uart_enable_instance(UART_SERCOM);
}

bool uart_is_enabled(void) {
    return uart_is_enabled_instance(UART_SERCOM);
}

void uart_write(char *data, size_t length) {
    uart_write_instance(UART_SERCOM, data, length);
}

size_t uart_read(char *data, size_t max_length) {
    return uart_read_instance(UART_SERCOM, data, max_length);
}

void uart_disable(void) {
    uart_disable_instance(UART_SERCOM);
}

#endif

void uart_init_instance(uint8_t sercom, uart_txpo_t txpo, uart_rxpo_t rxpo, uint32_t baud) {
    (void)txpo;
    (void)rxpo;

    if (sercom >= MAX_SERCOM_INSTANCES) return;

    uart_sim_state_t *state = &uart_instances[sercom];
    memset(state, 0, sizeof(uart_sim_state_t));
    state->baud = baud;
}

void uart_set_run_in_standby_instance(uint8_t sercom, bool run_in_standby) {
    (void)sercom;
    (void)run_in_standby;
}

void uart_set_irda_mode_instance(uint8_t sercom, bool irda) {
    if (sercom >= MAX_SERCOM_INSTANCES) return;
    uart_instances[sercom].irda_mode = irda;
}

void uart_enable_instance(uint8_t sercom) {
    if (sercom >= MAX_SERCOM_INSTANCES) return;
    uart_instances[sercom].enabled = true;
}

bool uart_is_enabled_instance(uint8_t sercom) {
    if (sercom >= MAX_SERCOM_INSTANCES) return false;
    return uart_instances[sercom].enabled;
}

void uart_write_instance(uint8_t sercom, char *data, size_t length) {
    (void)sercom;
    (void)data;
    (void)length;
    // In simulator, writing could log to console or send to UI
    // For now, just ignore writes
}

size_t uart_read_instance(uint8_t sercom, char *data, size_t max_length) {
    if (sercom >= MAX_SERCOM_INSTANCES) return 0;

    uart_sim_state_t *state = &uart_instances[sercom];

    size_t bytes_read = 0;
    while (bytes_read < max_length && state->rx_count > 0) {
        data[bytes_read++] = state->rx_buffer[state->rx_read_pos];
        state->rx_read_pos = (state->rx_read_pos + 1) % UART_BUFFER_SIZE;
        state->rx_count--;
    }

    return bytes_read;
}

bool uart_read_byte_instance(uint8_t sercom, char *byte) {
    return uart_read_instance(sercom, byte, 1) == 1;
}

void uart_disable_instance(uint8_t sercom) {
    if (sercom >= MAX_SERCOM_INSTANCES) return;
    uart_instances[sercom].enabled = false;
}

void uart_irq_handler(uint8_t sercom) {
    (void)sercom;
    // IRQ handler stub for simulator
}
