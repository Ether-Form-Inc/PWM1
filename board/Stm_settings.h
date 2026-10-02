// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ether Form Inc.
#pragma once

// Settings kept in the last pages of flash, which the linker script keeps the application out of.
// Each setting is one byte in its own page. Erased flash (all 0xFF) means "not set".

#include <stm32g0xx_hal.h>

#include <cstdint>

namespace Stm_settings
{
struct Setting
{
  uint32_t page;
  uint32_t address;
};

constexpr Setting node_id{62, 0x0801F000};     // node ID override, 1 to 127
constexpr Setting pwm_ranges{61, 0x0801E800};  // PWM pulse width limits, see pwm_range_count

// Reads a one byte setting. Returns false if it was never written (or holds 0xFF).
inline bool read(const Setting &s, uint8_t &value)
{
  const uint64_t raw = *(const volatile uint64_t *)s.address;
  value = (uint8_t)raw;
  return raw != 0xFFFFFFFFFFFFFFFFULL && value != 0xFF;
}

// Erases the setting, so it reads as "not set". Returns true on success.
inline bool erase(const Setting &s)
{
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR | FLASH_FLAG_PROGERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                         FLASH_FLAG_SIZERR | FLASH_FLAG_PGSERR | FLASH_FLAG_MISERR | FLASH_FLAG_FASTERR);

  FLASH_EraseInitTypeDef erase_init = {};
  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init.Banks = FLASH_BANK_1;
  erase_init.Page = s.page;
  erase_init.NbPages = 1;
  uint32_t page_error = 0;
  const bool ok = HAL_FLASHEx_Erase(&erase_init, &page_error) == HAL_OK;
  HAL_FLASH_Lock();
  return ok;
}

// Stores a one byte setting (the page is erased first). The CPU is stalled while flash is
// erased, a few tens of milliseconds. Returns true if the value reads back correctly.
inline bool write(const Setting &s, uint8_t value)
{
  if (!erase(s))
  {
    return false;
  }
  HAL_FLASH_Unlock();
  const bool ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, s.address, (uint64_t)value) == HAL_OK;
  HAL_FLASH_Lock();

  uint8_t check;
  return ok && read(s, check) && check == value;
}

// The PWM settings share one page of halfwords, 0xFFFF = not set (use the default). The first 16 are the pulse width
// limits in us in the order servo min, servo max, motor min and motor max, each for outputs 0 to 3. The 17th is the
// motor mode command timeout in ms. The count is a multiple of 4 because flash is written in 8 byte units.
constexpr uint32_t pwm_limit_count = 16;
constexpr uint32_t pwm_range_count = 20;

inline void read_pwm_ranges(uint16_t (&values)[pwm_range_count])
{
  for (uint32_t i = 0; i < pwm_range_count; i++)
  {
    values[i] = *(const volatile uint16_t *)(pwm_ranges.address + 2 * i);
  }
}

// Rewrites the whole page. The CPU is stalled while flash is erased, a few tens of milliseconds. Returns true if
// the values read back correctly.
inline bool write_pwm_ranges(const uint16_t (&values)[pwm_range_count])
{
  if (!erase(pwm_ranges))
  {
    return false;
  }
  HAL_FLASH_Unlock();
  bool ok = true;
  for (uint32_t d = 0; d < pwm_range_count / 4 && ok; d++)
  {
    uint64_t word = 0;
    for (uint32_t k = 0; k < 4; k++)
    {
      word |= (uint64_t)values[d * 4 + k] << (16 * k);
    }
    if (word != 0xFFFFFFFFFFFFFFFFULL)
    {
      ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, pwm_ranges.address + 8 * d, word) == HAL_OK;
    }
  }
  HAL_FLASH_Lock();

  uint16_t check[pwm_range_count];
  read_pwm_ranges(check);
  for (uint32_t i = 0; i < pwm_range_count; i++)
  {
    ok = ok && check[i] == values[i];
  }
  return ok;
}
} // namespace Stm_settings
