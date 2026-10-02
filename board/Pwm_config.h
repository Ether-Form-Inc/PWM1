// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ether Form Inc.
#pragma once

// The PWM pulse width limits and the motor timeout, and the arithmetic that turns commands into pulse widths. This
// has no hardware or CAN dependencies so it can be unit tested on the PC.
//
// There are 17 settings, kept as halfwords in one flash page (0xFFFF = not set, so the default is used):
//   0..3   servo minimum pulse width (us) for outputs 0 to 3
//   4..7   servo maximum pulse width (us)
//   8..11  motor minimum pulse width (us)
//   12..15 motor maximum pulse width (us)
//   16     motor mode command timeout (ms)
// The stored array is padded to 20 halfwords because flash is written in 8 byte units.

#include <user_defined_threshold.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Pwm
{
constexpr uint32_t num_outputs = 4;
constexpr uint32_t limit_count = 16;   // pulse width limits
constexpr uint32_t timeout_index = 16; // motor timeout
constexpr uint32_t setting_count = 20; // stored halfwords (17 used)
constexpr uint16_t unset = 0xFFFF;

constexpr int64_t limit_param_max = 20000; // us, one 50 Hz period
constexpr int64_t timeout_param_min = 100; // ms, the smallest value accepted (0 = default)
constexpr int64_t timeout_param_max = 60000;

constexpr uint32_t pwm_period_us = 20000; // 50 Hz
constexpr uint32_t pwm_counts = 4096;     // PCA9685 resolution
constexpr float motor_full_scale = 8192.f;

// Parameter names: NODE_ID is parameter 0, the settings above are parameters 1 to 17
constexpr uint32_t param_count = 1 + timeout_index + 1;
constexpr size_t param_name_max = 17; // "MOTOR_TIMEOUT_MS" and the terminator

// Writes the name of parameter `index` (0 = NODE_ID) into buf (param_name_max bytes). Returns false if there is no
// such parameter.
inline bool param_name(uint32_t index, char *buf)
{
  static const char *const prefixes[4] = {"SERVO_MIN_US_", "SERVO_MAX_US_", "MOTOR_MIN_US_", "MOTOR_MAX_US_"};
  if (index == 0)
  {
    strcpy(buf, "NODE_ID");
    return true;
  }
  if (index >= param_count)
  {
    return false;
  }
  const uint32_t i = index - 1;
  if (i == timeout_index)
  {
    strcpy(buf, "MOTOR_TIMEOUT_MS");
    return true;
  }
  strcpy(buf, prefixes[i / 4]);
  const size_t n = strlen(buf);
  buf[n] = (char)('0' + i % 4);
  buf[n + 1] = 0;
  return true;
}

// The parameter index for a name (not terminated), or -1
inline int find_param(const uint8_t *name, size_t len)
{
  char buf[param_name_max];
  for (uint32_t i = 0; i < param_count; i++)
  {
    if (param_name(i, buf) && strlen(buf) == len && memcmp(buf, name, len) == 0)
    {
      return (int)i;
    }
  }
  return -1;
}

inline uint32_t default_setting(uint32_t i)
{
  static const uint16_t *const tables[4] = {minimum_servo_us, maximum_servo_us, minimum_motor_us, maximum_motor_us};
  if (i == timeout_index)
  {
    return default_motor_timeout_ms;
  }
  return tables[i / 4][i % 4];
}

// Whether a stored value is acceptable for setting i (0xFFFF, "not set", always is)
inline bool stored_value_ok(uint32_t i, uint16_t v)
{
  if (v == unset)
  {
    return true;
  }
  if (i == timeout_index)
  {
    return v >= timeout_param_min && v <= timeout_param_max;
  }
  return v >= 1 && v <= limit_param_max;
}

// PCA9685 count (out of 4096 per 20 ms) for a pulse width
inline uint16_t pulse_to_count(uint16_t pulse_us)
{
  return (uint16_t)(((uint32_t)pulse_us * pwm_counts) / pwm_period_us);
}

class Config
{
public:
  Config()
  {
    clear();
  }

  // Back to the defaults
  void clear()
  {
    for (auto &v : m_stored)
    {
      v = unset;
    }
    apply();
  }

  // Loads the halfwords read from flash. Anything out of range counts as not set, and if the limits of any output
  // are inconsistent (maximum not above minimum) all the limits go back to their defaults.
  void load(const uint16_t (&stored)[setting_count])
  {
    memcpy(m_stored, stored, sizeof(m_stored));
    for (uint32_t i = 0; i < setting_count; i++)
    {
      if (i > timeout_index || !stored_value_ok(i, m_stored[i]) || m_stored[i] == 0)
      {
        m_stored[i] = unset;
      }
    }
    if (!valid(m_stored))
    {
      for (uint32_t i = 0; i < limit_count; i++)
      {
        m_stored[i] = unset;
      }
    }
    apply();
  }

  const uint16_t (&stored() const)[setting_count]
  {
    return m_stored;
  }

  // The value in use for setting i
  uint32_t effective(uint32_t i) const
  {
    return effective(i, m_stored);
  }

  // Builds the stored array that would result from writing `wanted` to setting i (0 = back to the default). Returns
  // false if the value is out of range, would leave an output with a maximum not above its minimum, or would not
  // change anything.
  bool propose(uint32_t i, int64_t wanted, uint16_t (&candidate)[setting_count]) const
  {
    if (i > timeout_index || wanted < 0 || wanted > 0xFFFE || (wanted != 0 && !stored_value_ok(i, (uint16_t)wanted)))
    {
      return false;
    }
    memcpy(candidate, m_stored, sizeof(candidate));
    candidate[i] = wanted == 0 ? unset : (uint16_t)wanted;
    return candidate[i] != m_stored[i] && valid(candidate);
  }

  // Makes the proposed array the one in use (after it has been stored in flash)
  void commit(const uint16_t (&candidate)[setting_count])
  {
    memcpy(m_stored, candidate, sizeof(m_stored));
    apply();
  }

  uint32_t timeout_ms() const
  {
    return m_timeout_ms;
  }

  uint16_t servo_min(uint32_t ch) const
  {
    return m_servo_min[ch];
  }
  uint16_t servo_max(uint32_t ch) const
  {
    return m_servo_max[ch];
  }
  uint16_t motor_min(uint32_t ch) const
  {
    return m_motor_min[ch];
  }
  uint16_t motor_max(uint32_t ch) const
  {
    return m_motor_max[ch];
  }

  // Pulse width (us) for a UNITLESS servo command (-1 to +1)
  uint16_t servo_unitless_us(uint32_t ch, float unitless) const
  {
    unitless = unitless < -1.f ? -1.f : (unitless > 1.f ? 1.f : unitless);
    const uint16_t range = m_servo_max[ch] - m_servo_min[ch];
    const uint16_t pulse = (uint16_t)(((unitless + 1.f) / 2.f) * range) + m_servo_min[ch];
    return servo_limited_us(ch, pulse);
  }

  // Pulse width (us) for a PWM servo command (us), limited to this output's servo range
  uint16_t servo_pwm_us(uint32_t ch, float us) const
  {
    const float clamped = us < 0.f ? 0.f : (us > 65535.f ? 65535.f : us);
    return servo_limited_us(ch, (uint16_t)clamped);
  }

  // Pulse width (us) for a motor command, 0 to 8191. Negative values are not supported and count as 0.
  uint16_t motor_us(uint32_t ch, int16_t cmd) const
  {
    cmd = cmd < 0 ? 0 : cmd;
    float fraction = static_cast<float>(cmd) / motor_full_scale;
    fraction = fraction > 1.f ? 1.f : fraction;
    const uint16_t range = m_motor_max[ch] - m_motor_min[ch];
    return (uint16_t)(fraction * static_cast<float>(range)) + m_motor_min[ch];
  }

private:
  uint16_t servo_limited_us(uint32_t ch, uint16_t pulse) const
  {
    pulse = pulse < m_servo_min[ch] ? m_servo_min[ch] : pulse;
    return pulse > m_servo_max[ch] ? m_servo_max[ch] : pulse;
  }

  static uint32_t effective(uint32_t i, const uint16_t *stored)
  {
    return stored[i] != unset ? stored[i] : default_setting(i);
  }

  // True if every output would have a maximum greater than its minimum with these stored values
  static bool valid(const uint16_t *stored)
  {
    // Servo limits start at index 0 and motor limits at index 8; the maxima are 4 after the minima
    for (uint32_t base = 0; base < limit_count; base += 8)
    {
      for (uint32_t c = 0; c < num_outputs; c++)
      {
        if (effective(base + 4 + c, stored) <= effective(base + c, stored))
        {
          return false;
        }
      }
    }
    return true;
  }

  void apply()
  {
    for (uint32_t c = 0; c < num_outputs; c++)
    {
      m_servo_min[c] = (uint16_t)effective(c, m_stored);
      m_servo_max[c] = (uint16_t)effective(4 + c, m_stored);
      m_motor_min[c] = (uint16_t)effective(8 + c, m_stored);
      m_motor_max[c] = (uint16_t)effective(12 + c, m_stored);
    }
    m_timeout_ms = effective(timeout_index, m_stored);
  }

  uint16_t m_stored[setting_count];
  uint16_t m_servo_min[num_outputs], m_servo_max[num_outputs];
  uint16_t m_motor_min[num_outputs], m_motor_max[num_outputs];
  uint32_t m_timeout_ms{default_motor_timeout_ms};
};
} // namespace Pwm
