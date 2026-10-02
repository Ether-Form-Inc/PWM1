// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ether Form Inc.
#pragma once

// Measures the period and the high time of a PWM output with a timer in input capture mode. The four PCA9685 outputs are
// also wired to MCU pins (output 0 to 3 on PA2 to PA5), so the MCU can time them without any extra hardware. This is
// how a board's PWM timing is calibrated at bring-up (see PWM_PRESCALE). The timer runs from the MCU's 16 MHz internal
// oscillator, accurate to about 1 %, so the result is a calibration reference, not a lab measurement.
//
//   output 0: PA2, TIM2 channel 3      output 2: PA4, TIM14 channel 1
//   output 1: PA3, TIM2 channel 4      output 3: PA5, TIM2 channel 1
//
// The measurement is polled and blocks for the measurement time (about 40 PWM periods, 0.8 s at 50 Hz).

#include <stm32g0xx_hal.h>

#include <cstdint>

class Stm_pwm_meter
{
public:
  struct Result
  {
    bool ok{false};          // enough pulses were seen
    uint32_t period_us{0};   // median time between rising edges
    uint32_t high_us{0};     // median time from a rising edge to the next falling edge
    uint32_t periods{0};     // how many periods the medians are from
  };

  static constexpr uint32_t num_outputs = 4;

  Result measure(uint32_t output, uint32_t timeout_ms = 1500)
  {
    Result result;
    if (output >= num_outputs)
    {
      return result;
    }
    const uint32_t pin = 2 + output;
    const bool use_tim14 = output == 2;
    TIM_TypeDef *tim = use_tim14 ? TIM14 : TIM2;
    const uint32_t channel = output == 0 ? 3 : (output == 1 ? 4 : 1);
    const uint32_t alternate_function = use_tim14 ? 4 : 2; // TIM14_CH1 is AF4 on PA4, the others are TIM2 AF2
    const uint32_t mask = use_tim14 ? 0xFFFFu : 0xFFFFFFFFu;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    if (use_tim14)
    {
      __HAL_RCC_TIM14_CLK_ENABLE();
    }
    else
    {
      __HAL_RCC_TIM2_CLK_ENABLE();
    }

    // The pin is an analog input for the ADC the rest of the time: switch it to the timer and put it back afterwards
    const uint32_t saved_moder = GPIOA->MODER;
    const uint32_t saved_afr = GPIOA->AFR[0];
    GPIOA->MODER = (saved_moder & ~(3u << (pin * 2))) | (2u << (pin * 2));
    GPIOA->AFR[0] = (saved_afr & ~(0xFu << (pin * 4))) | (alternate_function << (pin * 4));

    tim->CR1 = 0;
    tim->PSC = 15; // 16 MHz / 16 = 1 MHz, one tick per microsecond
    tim->ARR = mask;
    volatile uint32_t *ccmr = channel <= 2 ? &tim->CCMR1 : &tim->CCMR2;
    const uint32_t shift = ((channel - 1) & 1) * 8;
    *ccmr = (*ccmr & ~(0xFFu << shift)) | (0x01u << shift); // CCxS = 01: capture from the pin, no filter or prescaler
    tim->CCER = 0xBu << ((channel - 1) * 4);                // capture enabled, on both edges
    tim->EGR = TIM_EGR_UG;
    tim->SR = 0;
    tim->CR1 = TIM_CR1_CEN;
    volatile uint32_t *ccr = channel == 1 ? &tim->CCR1 : (channel == 2 ? &tim->CCR2 : (channel == 3 ? &tim->CCR3 : &tim->CCR4));

    uint32_t time[max_edges];
    uint8_t level[max_edges];
    uint32_t edges = 0;
    const uint32_t start_ms = HAL_GetTick();
    while (edges < max_edges && (uint32_t)(HAL_GetTick() - start_ms) < timeout_ms)
    {
      if (tim->SR & (1u << channel))
      {
        time[edges] = *ccr; // reading the capture register clears the flag
        level[edges] = (GPIOA->IDR >> pin) & 1;
        edges++;
      }
    }

    tim->CR1 = 0;
    tim->CCER = 0;
    GPIOA->MODER = saved_moder;
    GPIOA->AFR[0] = saved_afr;

    uint32_t periods[max_edges / 2];
    uint32_t highs[max_edges / 2];
    uint32_t period_count = 0;
    uint32_t high_count = 0;
    uint32_t last_rise = 0;
    bool have_rise = false;
    for (uint32_t i = 0; i < edges; i++)
    {
      if (level[i])
      {
        if (have_rise && period_count < max_edges / 2)
        {
          periods[period_count++] = (time[i] - last_rise) & mask;
        }
        last_rise = time[i];
        have_rise = true;
      }
      else if (have_rise && high_count < max_edges / 2)
      {
        highs[high_count++] = (time[i] - last_rise) & mask;
      }
    }
    result.periods = period_count;
    result.period_us = median(periods, period_count);
    result.high_us = median(highs, high_count);
    result.ok = period_count >= min_periods;
    return result;
  }

private:
  static constexpr uint32_t max_edges = 96; // 48 pulses
  static constexpr uint32_t min_periods = 8;

  // Sorts a small array in place and returns its median (0 if empty)
  static uint32_t median(uint32_t *values, uint32_t count)
  {
    if (count == 0)
    {
      return 0;
    }
    for (uint32_t i = 1; i < count; i++)
    {
      const uint32_t v = values[i];
      uint32_t j = i;
      while (j > 0 && values[j - 1] > v)
      {
        values[j] = values[j - 1];
        j--;
      }
      values[j] = v;
    }
    return values[count / 2];
  }
};
