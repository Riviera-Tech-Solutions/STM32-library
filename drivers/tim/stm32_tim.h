/** @file stm32_tim.h – Timer PWM API: channel 1 only. */
#ifndef STM32_TIM_H
#define STM32_TIM_H
#include <stdint.h>
#include <stdbool.h>
#ifndef TIM_INSTANCE_MASK
#define TIM_INSTANCE_MASK 3U
#endif
/* Instances 0=TIM2, 1=TIM3. Set mask for the exact part; channel must be 1.
 * Frequency must be nonzero. No input capture is implemented. */
bool tim_pwm_init(uint8_t instance, uint8_t channel, uint32_t freq_hz);
void tim_pwm_set_duty(uint8_t instance, uint8_t channel, uint8_t percent);
#endif
