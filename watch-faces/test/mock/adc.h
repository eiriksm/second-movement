#ifndef ADC_H
#define ADC_H

#include <stdint.h>

// The pin helpers the faces use around ADC reads do nothing in tests.
#define HAL_GPIO_PMUX_ADC 0
#define HAL_GPIO_IR_ENABLE_out() ((void)0)
#define HAL_GPIO_IR_ENABLE_clr() ((void)0)
#define HAL_GPIO_IR_ENABLE_off() ((void)0)
#define HAL_GPIO_IRSENSE_pmuxen(mux) ((void)(mux))
#define HAL_GPIO_IRSENSE_pmuxdis() ((void)0)
#define HAL_GPIO_IRSENSE_off() ((void)0)
#define HAL_GPIO_IRSENSE_pin() 0

void adc_init(void);
void adc_enable(void);
void adc_disable(void);
uint16_t adc_get_analog_value(uint16_t pin);

// Sets the value every following adc_get_analog_value call returns.
void adc_mock_set_value(uint16_t value);

#endif // ADC_H
