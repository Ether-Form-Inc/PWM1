#pragma once

#include <Can_to_pwm_translator.h>
#include <user_defined_threshold.h>

using namespace uavcan;

template <typename CAN_IF, typename SYS_CLK>
class STM_can_to_pwm : public Can_to_pwm_translator<CAN_IF, SYS_CLK>
{
  static constexpr uint32_t NUM_PWM_OUTPUTS{4};
  static constexpr uint8_t PCA9685_ADDR{0x43};

  typedef uavcan::equipment::actuator::ArrayCommand servo_handler_type_t;
  typedef uavcan::equipment::esc::RawCommand motor_handler_type_t;
  typedef uavcan::equipment::actuator::Command servo_command_t;

  typedef Can_to_pwm_translator<CAN_IF, SYS_CLK> Base;

public:
  STM_can_to_pwm(uint32_t actuator_id, bool is_servo, uint8_t node_id)
      : Base(actuator_id, is_servo, 0, node_id)

  {
    // Initialize hi2c2 constants
    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x00503D58;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    m_initialized = ((HAL_I2C_Init(&hi2c2) == HAL_OK));

    // Configure Analogue filter
    m_initialized &= (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) == HAL_OK);

    // Configure Digital filter
    m_initialized &= (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) == HAL_OK);

    // Initialize PCA
    m_initialized &= PCA9685_Init();

    // Compute range of servo/motor us for unitless conversion
    for (uint32_t c = 0; c < 4; c++)
    {
      m_servo_range_us[c] = maximum_servo_us[c] - minimum_servo_us[c];
    }

    for (uint32_t c = 0; c < 4; c++)
    {
      m_motor_range_us[c] = maximum_motor_us[c] - minimum_motor_us[c];
    }
  }

  // The HW pins will determine actuator id offset. But it is assumed that ch 0-3 have ids = offset + (0-3)

  void handle_servo_set(const ReceivedDataStructure<servo_handler_type_t> &data) override
  {
    const uint32_t num_commands = data.commands.size();
    for (uint32_t c = 0; c < num_commands; c++)
    {
      servo_command_t cmd = data.commands[c];

      // This means that one of the local actuators should be modified based on this command
      if (cmd.actuator_id >= Base::get_actuator_id())
      {
        const uint32_t local_pwm_output{cmd.actuator_id - Base::get_actuator_id()};
        if (local_pwm_output < NUM_PWM_OUTPUTS)
        {
          if (cmd.command_type == servo_command_t::COMMAND_TYPE_PWM)
          {
            uint16_t v = cmd.command_value;
            set_us_servo(cmd.actuator_id - Base::get_actuator_id(), v);
          }
          if (cmd.command_type == servo_command_t::COMMAND_TYPE_UNITLESS)
          {
            float v = cmd.command_value;
            set_unitless_servo(cmd.actuator_id - Base::get_actuator_id(), v);
          }
        }
      }
    }
  }

  void handle_motor_set(const ReceivedDataStructure<motor_handler_type_t> &data) override
  {
    const uint32_t num_commands = data.cmd.size();

    // Stop at the minimum of maximum internal id or size of data
    const uint32_t base_actuator_id{Base::get_actuator_id()};
    const uint32_t loop_until{num_commands > base_actuator_id + 4 ? base_actuator_id + 4 : num_commands};

    // c is actuator id
    for (uint32_t c = base_actuator_id; c < loop_until; c++)
    {
      int16_t motor_cmd = data.cmd[c];
      //(data.cmd[c] & 0xfffc) / 4; // Clear bottom two bits and then "bit shift" 2 bits down excluding sign bit

      // This means that one of the local actuators should be modified based on this command
      const uint32_t local_pwm_output{c - base_actuator_id};
      set_motor(local_pwm_output, motor_cmd);
    }
  }

  void handle_read_pwm() override
  {
  }

  void delay(uint64_t delay_us) override
  {
    HAL_Delay(delay_us / 1000);
  }

private:
  I2C_HandleTypeDef hi2c2;
  volatile bool m_initialized{false};
  uint16_t m_servo_range_us[NUM_PWM_OUTPUTS];
  uint16_t m_motor_range_us[NUM_PWM_OUTPUTS];

  bool PCA9685_Init(void)
  {
    bool ret{true};
    uint8_t data[2];

    // MODE1: sleep
    data[0] = 0x00;
    data[1] = 0x10;
    ret &= (HAL_I2C_Master_Transmit(&hi2c2, PCA9685_ADDR << 1, data, 2, 100) == HAL_OK);

    // PRE_SCALE: 50Hz servo PWM
    // 25MHz oscillator / (4096 * 50Hz) - 1 = 121
    data[0] = 0xFE;
    data[1] = 121;
    ret &= (HAL_I2C_Master_Transmit(&hi2c2, PCA9685_ADDR << 1, data, 2, 100) == HAL_OK);

    // MODE1: wake up, auto increment
    data[0] = 0x00;
    data[1] = 0x20;
    ret &= (HAL_I2C_Master_Transmit(&hi2c2, PCA9685_ADDR << 1, data, 2, 100) == HAL_OK);

    return ret;
  }

  static uint16_t convert_pulse_to_count(uint16_t pulse_us)
  {
    return ((pulse_us * 4096ul) / 20000ul);
  }

  void set_pwm(uint8_t channel_x, uint16_t on, uint16_t off)
  {
    uint8_t pwm_val[4];
    pwm_val[0] = on & 0xFF;
    pwm_val[1] = (on >> 8) & 0x0F;
    pwm_val[2] = off & 0xFF;
    pwm_val[3] = (off >> 8) & 0x0F;

    static constexpr uint8_t ch0_addr{0x06};
    const uint8_t ch_x_addr{(uint8_t)(ch0_addr + channel_x * 4)};
    HAL_I2C_Mem_Write(&hi2c2, PCA9685_ADDR << 1, ch_x_addr, I2C_MEMADD_SIZE_8BIT, pwm_val, 4, 1000);
  }

  void set_unitless_servo(uint8_t channel_x, float unitless)
  {
    uint16_t pulse_us =
        static_cast<uint16_t>(((unitless + 1.f) / 2.f) * m_servo_range_us[channel_x]) + minimum_servo_us[channel_x];
    set_us_servo(channel_x, pulse_us);
  }

  void set_us_servo(uint8_t channel_x, uint16_t pulse_us)
  {
    const uint16_t on{0};
    const uint16_t off{convert_pulse_to_count(pulse_us)};
    set_pwm(channel_x, on, off);
  }

  void set_motor(uint8_t channel_x, int16_t cmd)
  {
    // We need to map an int14 to the pwm range
    constexpr int16_t maximum_motor_cmd{(1 << 13) - 1};
    constexpr int16_t minimum_motor_cmd{-(1 << 13)};
    constexpr uint16_t motor_range{(uint16_t)(maximum_motor_cmd - minimum_motor_cmd)};

    uint16_t positive_cmd = static_cast<uint16_t>(cmd + (int16_t)(2 << 12));
    float val_thresh = static_cast<float>(positive_cmd) / static_cast<float>(motor_range);
    uint16_t mapped_pwm_offset_from_min =
        static_cast<uint16_t>(val_thresh * static_cast<float>(m_motor_range_us[channel_x]));

    uint16_t pwm_pulse_us = mapped_pwm_offset_from_min + minimum_motor_us[channel_x];

    const uint16_t on{0};
    const uint16_t off{convert_pulse_to_count(pwm_pulse_us)};
    set_pwm(channel_x, on, off);
  }
};
