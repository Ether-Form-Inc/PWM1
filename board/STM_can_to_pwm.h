#include <Can_to_pwm_translator.h>

using namespace uavcan;

#define PCA9685_ADDR (0x43 << 1) // HAL expects 8-bit address

template <typename CAN_IF, typename SYS_CLK> class STM_can_to_pwm : public Can_to_pwm_translator<CAN_IF, SYS_CLK>
{
  typedef uavcan::equipment::actuator::ArrayCommand servo_handler_type_t;
  typedef uavcan::equipment::esc::RawCommand motor_handler_type_t;

  typedef Can_to_pwm_translator<CAN_IF, SYS_CLK> Base;

  static constexpr uint16_t pwm_id{0x43};

  void PCA9685_Init(void)
  {
    uint8_t data[2];

    // MODE1: sleep
    data[0] = 0x00;
    data[1] = 0x10;
    HAL_I2C_Master_Transmit(&hi2c2, PCA9685_ADDR, data, 2, 100);

    // PRE_SCALE: 50Hz servo PWM
    // 25MHz oscillator / (4096 * 50Hz) - 1 = 121
    data[0] = 0xFE;
    data[1] = 121;
    HAL_I2C_Master_Transmit(&hi2c2, PCA9685_ADDR, data, 2, 100);

    // MODE1: wake up, auto increment
    data[0] = 0x00;
    data[1] = 0x20;
    HAL_I2C_Master_Transmit(&hi2c2, PCA9685_ADDR, data, 2, 100);

    HAL_Delay(5);
  }

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
    PCA9685_Init();

    // uint8_t sleep = 0x20 | 0x10;
    //(void)HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, 0x00, I2C_MEMADD_SIZE_8BIT, &sleep, 1, 1000);

    // uint8_t mode2 = 0x04; // totem pole
    //(void)HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, 0x01, I2C_MEMADD_SIZE_8BIT, &mode2, 1, 1000);

    // uint8_t prescale = 0x79;
    //(void)HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, 0xFE, I2C_MEMADD_SIZE_8BIT, &prescale, 1, 1000);

    // uint8_t wake = 0x20;
    //(void)HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, 0x00, I2C_MEMADD_SIZE_8BIT, &wake, 1, 1000);
  }

  uint16_t convert_pulse_to_count(uint16_t pulse_us)
  {
    return ((pulse_us * 4096ul) / 20000ul);
  }

  void handle_servo_set(const ReceivedDataStructure<servo_handler_type_t> &data) override
  {
    auto cmd = data.commands[0];
    if (cmd.actuator_id == Base::get_actuator_id())
    {
      // std::cout << "Setting servo 0 to: " << cmd.command_value;
      uint16_t v = cmd.command_value;

      // auto v{cmd.command_value};
      const uint16_t on{0};
      const uint16_t off{convert_pulse_to_count(v)};

      uint8_t pwm_val[4];
      pwm_val[0] = on & 0xFF;
      pwm_val[1] = (on >> 8) & 0x0F;
      pwm_val[2] = off & 0xFF;
      pwm_val[3] = (off >> 8) & 0x0F;

      uint8_t addr1{0xfa}; // All led
      uint8_t addr2{0x06}; // led 0
      uint8_t addr3{0x0a}; // led 1
      uint8_t addr4{0x0e}; // led 2
      uint8_t addr5{0x12}; // led 3

      // HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, addr1, I2C_MEMADD_SIZE_8BIT, pwm_val, 4, 1000);
      // HAL_Delay(5);
      HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, addr2, I2C_MEMADD_SIZE_8BIT, pwm_val, 4, 1000);
      HAL_Delay(5);
      HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, addr3, I2C_MEMADD_SIZE_8BIT, pwm_val, 4, 1000);
      HAL_Delay(5);
      HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, addr4, I2C_MEMADD_SIZE_8BIT, pwm_val, 4, 1000);
      HAL_Delay(5);
      HAL_I2C_Mem_Write(&hi2c2, pwm_id << 1, addr5, I2C_MEMADD_SIZE_8BIT, pwm_val, 4, 1000);
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
