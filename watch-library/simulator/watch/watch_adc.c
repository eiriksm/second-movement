/*
 * MIT License
 *
 * Copyright (c) 2020 Joey Castillo
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "watch_adc.h"
#include <emscripten.h>

static uint16_t _sim_get_light_adc_value(void) {
    // Read the simulated lux value and map it to a 16-bit ADC range.
    // OPT3001 max range is ~83865 lux; we scale proportionally.
    double lux = EM_ASM_DOUBLE({
        return window.light_lux || 0.0;
    });
    if (lux <= 0) return 0;
    if (lux >= 83865.0) return 65535;
    return (uint16_t)(lux * 65535.0 / 83865.0);
}

void watch_enable_adc(void) {}

void watch_enable_analog_input(const uint16_t pin) {
    (void) pin;
}

uint16_t watch_get_analog_pin_level(const uint16_t pin) {
    (void) pin;
    return _sim_get_light_adc_value();
}

void watch_set_analog_num_samples(uint16_t samples) {
    (void) samples;
}

void watch_set_analog_sampling_length(uint8_t cycles) {
    (void) cycles;
}

void watch_set_analog_reference_voltage(uint8_t reference) {
    (void) reference;
}

uint16_t watch_get_vcc_voltage(void) {
    // TODO: (a2) hook to UI
    return 3000;
}

inline void watch_disable_analog_input(const uint16_t pin) {
    (void) pin;
}

inline void watch_disable_adc(void) {}

// Gossamer-level ADC stubs: these are the low-level peripheral functions
// that some watch faces call directly (e.g. light_sensor_face).

void adc_init(void) {}

void adc_enable(void) {}

void adc_disable(void) {}

uint16_t adc_get_analog_value(uint16_t pin) {
    (void) pin;
    return _sim_get_light_adc_value();
}
