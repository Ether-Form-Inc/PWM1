#pragma once

#include <Can_to_pwm_translator.h>
#include <Stm_settings.h>
#include <user_defined_threshold.h>

#include <uavcan/protocol/param_server.hpp>
#include <uavcan/protocol/restart_request_server.hpp>

using namespace uavcan;

template <typename CAN_IF, typename SYS_CLK> class STM_can_to_pwm : public Can_to_pwm_translator<CAN_IF, SYS_CLK>
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
      , m_is_servo(is_servo)
      , m_param_server(Base::m_ownship_node)
      , m_restart_server(Base::m_ownship_node)
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
    // Any RawCommand message restarts the motor timeout
    m_last_motor_msg_ms = HAL_GetTick();
    m_motor_outputs_active = true;

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

  // Standard DroneCAN configuration services so the node ID can be set over CAN (for example from the DroneCAN GUI
  // tool): write the integer parameter NODE_ID (1 to 127) with uavcan.protocol.param.GetSet. It is stored in flash and
  // used from the next start; 0 erases it so the default node ID is used again.
  void custom_initialization() override
  {
    Base::custom_initialization();
    m_param_server.start(&m_param_manager);
    m_restart_server.setHandler(&m_restart_handler);
    m_restart_server.start();
  }

  void custom_broadcast() override
  {
    // Reset once the RestartNode response has had time to be sent
    if (m_restart_handler.requested && (HAL_GetTick() - m_restart_handler.requested_ms) >= restart_delay_ms)
    {
      NVIC_SystemReset();
    }

    // Motor mode only: switch the PWM signals off if no RawCommand has arrived for motor_timeout_ms. They come back
    // with the next RawCommand. Servo outputs hold their last value.
    if (!m_is_servo && m_motor_outputs_active && (HAL_GetTick() - m_last_motor_msg_ms) >= motor_timeout_ms)
    {
      for (uint8_t c = 0; c < NUM_PWM_OUTPUTS; c++)
      {
        set_pwm_off(c);
      }
      m_motor_outputs_active = false;
    }
  }

  void delay(uint64_t delay_us) override
  {
    HAL_Delay(delay_us / 1000);
  }

  private:
  static constexpr uint32_t restart_delay_ms{500};
  static constexpr uint32_t motor_timeout_ms{5000};
  static constexpr int64_t node_id_param_max{127};

  class Param_manager : public uavcan::IParamManager
  {
    static constexpr const char *node_id_name{"NODE_ID"};

    static int64_t stored_node_id()
    {
      uint8_t v;
      return Stm_settings::read(Stm_settings::node_id, v) ? v : 0;
    }

    void getParamNameByIndex(Index index, Name &out_name) const override
    {
      if (index == 0)
      {
        out_name = node_id_name;
      }
    }

    void assignParamValue(const Name &name, const Value &value) override
    {
      if (name == node_id_name && value.is(Value::Tag::integer_value))
      {
        const int64_t wanted = *value.as<Value::Tag::integer_value>();
        // An out of range value is ignored; the response shows what is actually stored
        if (wanted >= 0 && wanted <= node_id_param_max && wanted != stored_node_id())
        {
          if (wanted == 0)
          {
            Stm_settings::erase(Stm_settings::node_id);
          }
          else
          {
            Stm_settings::write(Stm_settings::node_id, (uint8_t)wanted);
          }
        }
      }
    }

    void readParamValue(const Name &name, Value &out_value) const override
    {
      if (name == node_id_name)
      {
        out_value.to<Value::Tag::integer_value>() = stored_node_id();
      }
    }

    void readParamDefaultMaxMin(const Name &name, Value &out_default, NumericValue &out_max,
                                NumericValue &out_min) const override
    {
      if (name == node_id_name)
      {
        out_default.to<Value::Tag::integer_value>() = 0;
        out_max.to<NumericValue::Tag::integer_value>() = node_id_param_max;
        out_min.to<NumericValue::Tag::integer_value>() = 0;
      }
    }

    // GetSet already stores the value in flash
    int saveAllParams() override
    {
      return 0;
    }

    // Back to the default node ID
    int eraseAllParams() override
    {
      return Stm_settings::erase(Stm_settings::node_id) ? 0 : -1;
    }
  };

  struct Restart_handler : public uavcan::IRestartRequestHandler
  {
    bool requested{false};
    uint32_t requested_ms{0};

    bool handleRestartRequest(uavcan::NodeID) override
    {
      requested = true;
      requested_ms = HAL_GetTick();
      return true;
    }
  };

  const bool m_is_servo;
  uint32_t m_last_motor_msg_ms{0};
  bool m_motor_outputs_active{false};

  Param_manager m_param_manager;
  Restart_handler m_restart_handler;
  uavcan::ParamServer m_param_server;
  uavcan::RestartRequestServer m_restart_server;

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

  // Holds the output low (PCA9685 "full off" bit), as it is after power-up
  void set_pwm_off(uint8_t channel_x)
  {
    uint8_t pwm_val[4] = {0, 0, 0, 0x10};
    static constexpr uint8_t ch0_addr{0x06};
    HAL_I2C_Mem_Write(&hi2c2, PCA9685_ADDR << 1, (uint8_t)(ch0_addr + channel_x * 4), I2C_MEMADD_SIZE_8BIT, pwm_val, 4,
                      1000);
  }

  void set_unitless_servo(uint8_t channel_x, float unitless)
  {
    // Clamp so out-of-range commands cannot produce out-of-range pulse widths
    unitless = unitless < -1.f ? -1.f : (unitless > 1.f ? 1.f : unitless);
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
    // constexpr int16_t maximum_motor_cmd{(1 << 13) - 1};
    // constexpr int16_t minimum_motor_cmd{0}; //-(1 << 13)};
    // constexpr uint16_t motor_range{(uint16_t)(maximum_motor_cmd - minimum_motor_cmd)};

    // uint16_t positive_cmd = static_cast<uint16_t>(cmd + (int16_t)(1 << 13));
    // float val_thresh = static_cast<float>(positive_cmd) / static_cast<float>(motor_range);

    // Negative commands are not supported (there is no meaningful PWM for them), so they are treated as 0
    cmd = cmd < 0 ? 0 : cmd;
    float val_thresh = static_cast<float>(cmd) / 8192.f;
    val_thresh = val_thresh > 1.f ? 1.f : val_thresh;

    uint16_t mapped_pwm_offset_from_min =
        static_cast<uint16_t>(val_thresh * static_cast<float>(m_motor_range_us[channel_x]));

    uint16_t pwm_pulse_us = mapped_pwm_offset_from_min + minimum_motor_us[channel_x];

    const uint16_t on{0};
    const uint16_t off{convert_pulse_to_count(pwm_pulse_us)};
    set_pwm(channel_x, on, off);
  }
};
