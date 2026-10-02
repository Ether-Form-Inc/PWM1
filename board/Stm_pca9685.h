// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ether Form Inc.
#pragma once

// Driver for the PCA9685 PWM controller on I2C2: 50 Hz, 12 bit, 4 outputs. A failed transfer marks the chip as not
// answering; writes are then skipped (they would only stall the main loop) and service() re-initializes the I2C
// peripheral and the chip every second. When it answers again the outputs are put back to their last state.

#include <stm32g0xx_hal.h>

#include <cstdint>

// The count last written to each output (0xFFFF = held off), readable over SWD for bench testing
inline volatile uint16_t pwm_debug_counts[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

class Stm_pca9685
{
public:
  static constexpr uint32_t num_outputs = 4;
  static constexpr uint16_t off = 0xFFFF; // last_count() value for "output held off"

  // (Re)initializes the I2C peripheral and the PCA9685. Returns true if the chip answered every write.
  bool init()
  {
    hi2c.Instance = I2C2;
    hi2c.Init.Timing = 0x00503D58;
    hi2c.Init.OwnAddress1 = 0;
    hi2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c.Init.OwnAddress2 = 0;
    hi2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    HAL_I2C_DeInit(&hi2c); // also clears a hung bus state left by an earlier error
    bool good = HAL_I2C_Init(&hi2c) == HAL_OK;
    good &= HAL_I2CEx_ConfigAnalogFilter(&hi2c, I2C_ANALOGFILTER_ENABLE) == HAL_OK;
    good &= HAL_I2CEx_ConfigDigitalFilter(&hi2c, 0) == HAL_OK;

    uint8_t data[2];
    // MODE1: sleep
    data[0] = 0x00;
    data[1] = 0x10;
    good &= HAL_I2C_Master_Transmit(&hi2c, addr << 1, data, 2, i2c_timeout_ms) == HAL_OK;
    // PRE_SCALE: 50 Hz servo PWM, 25 MHz oscillator / (4096 * 50 Hz) - 1 = 121
    data[0] = 0xFE;
    data[1] = 121;
    good &= HAL_I2C_Master_Transmit(&hi2c, addr << 1, data, 2, i2c_timeout_ms) == HAL_OK;
    // MODE1: wake up, auto increment
    data[0] = 0x00;
    data[1] = 0x20;
    good &= HAL_I2C_Master_Transmit(&hi2c, addr << 1, data, 2, i2c_timeout_ms) == HAL_OK;
    // The oscillator needs 500 us to start after sleep is cleared
    HAL_Delay(1);

    m_ok = good;
    m_last_try_ms = HAL_GetTick();
    return good;
  }

  bool ok() const
  {
    return m_ok;
  }

  // Sets an output to be high for `count` of the 4096 counts in each 20 ms period (0 on, `count` off)
  void set_count(uint8_t channel, uint16_t count)
  {
    m_last_count[channel] = count;
    pwm_debug_counts[channel] = count;
    write(channel, 0, 0, count & 0xFF, (count >> 8) & 0x0F);
  }

  // Holds an output low (PCA9685 "full off" bit), as it is after power-up
  void set_off(uint8_t channel)
  {
    m_last_count[channel] = off;
    pwm_debug_counts[channel] = off;
    write(channel, 0, 0, 0, 0x10);
  }

  uint16_t last_count(uint8_t channel) const
  {
    return m_last_count[channel];
  }

  // Call regularly. While the chip is not answering, retries init() once a second and restores the outputs when it
  // does. Returns true on the call where it recovered.
  bool service(uint32_t now_ms)
  {
    if (m_ok || (uint32_t)(now_ms - m_last_try_ms) < retry_ms)
    {
      return false;
    }
    if (!init())
    {
      return false;
    }
    for (uint8_t c = 0; c < num_outputs; c++)
    {
      if (m_last_count[c] == off)
      {
        set_off(c);
      }
      else
      {
        set_count(c, m_last_count[c]);
      }
    }
    return m_ok;
  }

private:
  static constexpr uint8_t addr = 0x43;
  static constexpr uint32_t i2c_timeout_ms = 20; // a transfer of a few bytes takes well under 1 ms
  static constexpr uint32_t retry_ms = 1000;
  static constexpr uint8_t ch0_reg = 0x06;

  // Writes the four bytes of a channel's ON/OFF registers. A failure marks the chip as not answering.
  void write(uint8_t channel, uint8_t on_l, uint8_t on_h, uint8_t off_l, uint8_t off_h)
  {
    if (!m_ok)
    {
      return;
    }
    uint8_t bytes[4] = {on_l, on_h, off_l, off_h};
    const uint8_t reg = (uint8_t)(ch0_reg + channel * 4);
    if (HAL_I2C_Mem_Write(&hi2c, addr << 1, reg, I2C_MEMADD_SIZE_8BIT, bytes, 4, i2c_timeout_ms) != HAL_OK)
    {
      m_ok = false;
      m_last_try_ms = HAL_GetTick();
    }
  }

  I2C_HandleTypeDef hi2c{};
  bool m_ok{false};
  uint32_t m_last_try_ms{0};
  uint16_t m_last_count[num_outputs] = {off, off, off, off};
};
