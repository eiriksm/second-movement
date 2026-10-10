#include "adc.h"

static uint16_t adc_value = 0xFFFF;

void adc_init(void) {}

void adc_enable(void) {}

void adc_disable(void) {}

uint16_t adc_get_analog_value(uint16_t pin) {
    (void)pin;
    return adc_value;
}

void adc_mock_set_value(uint16_t value) {
    adc_value = value;
}
