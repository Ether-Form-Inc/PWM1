#include <ICan_translator.h>

template <typename CAN_IF, typename SYS_CLK> class Can_to_pwm_translator : public ICan_translator<CAN_IF, SYS_CLK>
{
  typedef etherform::Pwm servo_handler_type_t;
  typedef etherform::Pwm motor_handler_type_t;

  typedef ICan_translator<CAN_IF, SYS_CLK> Base;

  public:
  Can_to_pwm_translator(bool is_servo, uint32_t hw_version, uint8_t node_id, std::string name = "CAN to PWM",
                        uint32_t sw_version = 0, uint8_t node_priority = 10)
      : Base(name, sw_version, hw_version, node_id, node_priority)
      , sub_pwm_servo(Base::m_ownship_node)
      , sub_pwm_motor(Base::m_ownship_node)
      , m_is_servo(is_servo)

  {
  }

  virtual ~Can_to_pwm_translator() override = default;

  void handle_servo(const ReceivedDataStructure<servo_handler_type_t> &data)
  {
  }

  void handle_motor(const ReceivedDataStructure<motor_handler_type_t> &data)
  {
  }

  void custom_initialization() override
  {
    if (m_is_servo)
    {
      std::function<void(const ReceivedDataStructure<servo_handler_type_t> &)> servo_func = [this](const auto &data)
      { this->handle_servo(data); };
      sub_pwm_servo.start(servo_func);
      Base::set_node_priority(20);
    }
    else
    {
      std::function<void(const ReceivedDataStructure<motor_handler_type_t> &)> motor_func = [this](const auto &data)
      { this->handle_motor(data); };
      sub_pwm_motor.start(motor_func);
      Base::set_node_priority(1);
    }
  }

  private:
  uavcan::Subscriber<etherform::Pwm> sub_pwm_servo;
  uavcan::Subscriber<etherform::Pwm> sub_pwm_motor;

  bool m_is_servo{true};
};
