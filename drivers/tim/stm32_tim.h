/** @file stm32_tim.h – Timer (PWM / input capture) API (stub). */
#ifndef STM32_TIM_H
#define STM32_TIM_H
#include <stdint.h>
#include <stdbool.h>
bool tim_pwm_init(uint8_t instance, uint8_t channel, uint32_t freq_hz);
void tim_pwm_set_duty(uint8_t instance, uint8_t channel, uint8_t percent);
#endif
