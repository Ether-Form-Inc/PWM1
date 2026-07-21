#include <Can_to_pwm_translator.h>

// #include <iostream>

using namespace uavcan;

template <typename CAN_IF, typename SYS_CLK> class STM_can_to_pwm : public Can_to_pwm_translator<CAN_IF, SYS_CLK>
{
  typedef uavcan::equipment::actuator::ArrayCommand servo_handler_type_t;
  typedef uavcan::equipment::esc::RawCommand motor_handler_type_t;

  typedef Can_to_pwm_translator<CAN_IF, SYS_CLK> Base;

  static constexpr uint16_t pwm_id{0x43};

  public:
  STM_can_to_pwm(uint32_t actuator_id, bool is_servo, uint8_t node_id)
      : Base(actuator_id, is_servo, 0, node_id)
  {
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

    /** Configure Analogue filter
     */
    m_initialized &= (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) == HAL_OK);

    /** Configure Digital filter
     */
    m_initialized &= (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) == HAL_OK);

    uint8_t sleep = 0x20 | 0x10;
    (void)HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, 0x00, I2C_MEMADD_SIZE_8BIT, &sleep, 1, 1000);

    uint8_t mode2 = 0x04; // totem pole
    (void)HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, 0x01, I2C_MEMADD_SIZE_8BIT, &mode2, 1, 1000);

    uint8_t prescale = 0x79;
    (void)HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, 0xFE, I2C_MEMADD_SIZE_8BIT, &prescale, 1, 1000);

    uint8_t wake = 0x20;
    (void)HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, 0x00, I2C_MEMADD_SIZE_8BIT, &wake, 1, 1000);
  }

  void handle_servo_set(const ReceivedDataStructure<servo_handler_type_t> &data) override
  {
    auto cmd = data.commands[0];
    if (cmd.actuator_id == Base::get_actuator_id())
    {
      // std::cout << "Setting servo 0 to: " << cmd.command_value;
      int32_t pwm_val = (int32_t)cmd.command_value;

      uint8_t addr{0xfa};
      // TODO: I think this is a switch case for actuator id
      //   case 0x50:
      //     addr = 0xFA;
      //     break;
      //   case 0x51:
      //     addr = 0x06;
      //     break;
      //   case 0x52:
      //     addr = 0x0A;
      //     break;
      //   case 0x53:
      //     addr = 0x0E;
      //     break;
      //   case 0x54:
      //     addr = 0x12;
      HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, addr, I2C_MEMADD_SIZE_8BIT, reinterpret_cast<uint8_t *>(&pwm_val), 4,
                        1000);
    }
  }

  void handle_motor_set(const ReceivedDataStructure<motor_handler_type_t> &data) override
  {
    // std::cout << "Posix motor handle called" << std::endl;
  }

  void handle_read_pwm() override
  {
  }

  private:
  int32_t pwm_val{-1};
  I2C_HandleTypeDef hi2c2;
  bool m_initialized{false};
};
