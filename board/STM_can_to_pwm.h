#pragma once

#include <Can_to_pwm_translator.h>
#include <user_defined_threshold.h>

using namespace uavcan;

template <typename CAN_IF, typename SYS_CLK> class STM_can_to_pwm : public Can_to_pwm_translator<CAN_IF, SYS_CLK>
{
  static constexpr uint8_t PCA9685_ADDR{0x43};
  typedef uavcan::equipment::actuator::ArrayCommand servo_handler_type_t;
  typedef uavcan::equipment::esc::RawCommand motor_handler_type_t;
  typedef uavcan::equipment::actuator::Command servo_command_t;

  typedef Can_to_pwm_translator<CAN_IF, SYS_CLK> Base;

  const uint16_t m_servo_range_us;

  public:
  STM_can_to_pwm(uint32_t actuator_id, bool is_servo, uint8_t node_id)
      : Base(actuator_id, is_servo, 0, node_id)
      , m_servo_range_us(maximum_servo_us - minimum_servo_us) // Compute range of servo us for unitless conversion

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
  }

  // The HW pins will determine actuator id offset. But it is assumed that ch 0-3 have ids = offset + (0-3)

  void handle_servo_set(const ReceivedDataStructure<servo_handler_type_t> &data) override
  {
    const uint32_t num_commands = data.commands.size() > 4 ? 4 : data.commands.size();
    for (uint32_t c = 0; c < num_commands; c++)
    {
      auto cmd = data.commands[c];
      // This means that one of the local actuators should be modified based on this command
      if (cmd.actuator_id + 4 > Base::get_actuator_id())
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

  void handle_motor_set(const ReceivedDataStructure<motor_handler_type_t> &data) override
  {
  }

  void handle_read_pwm() override
  {
  }

  private:
  I2C_HandleTypeDef hi2c2;
  volatile bool m_initialized{false};

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

  void set_servo(uint8_t channel_x, uint16_t on, uint16_t off)
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
    uint16_t pulse_us = static_cast<uint16_t>(((unitless + 1.f) / 2.f) * m_servo_range_us) + minimum_servo_us;
    set_us_servo(channel_x, pulse_us);
  }

  void set_us_servo(uint8_t channel_x, uint16_t pulse_us)
  {
    const uint16_t on{0};
    const uint16_t off{convert_pulse_to_count(pulse_us)};
    set_servo(channel_x, on, off);
  }
};
