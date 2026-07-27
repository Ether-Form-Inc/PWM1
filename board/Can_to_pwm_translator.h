// libuavcan utilizes enum arithmetic
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"

#include <ICan_translator.h>
#include <uavcan/equipment/actuator/ArrayCommand.hpp>
#include <uavcan/equipment/esc/RawCommand.hpp>

template <typename CAN_IF, typename SYS_CLK> class Can_to_pwm_translator : public ICan_translator<CAN_IF, SYS_CLK>
{
  typedef uavcan::equipment::actuator::ArrayCommand servo_handler_type_t;
  typedef uavcan::equipment::esc::RawCommand motor_handler_type_t;

  typedef ICan_translator<CAN_IF, SYS_CLK> Base;

  public:
  Can_to_pwm_translator(uint32_t actuator_id, bool is_servo, uint32_t hw_version, uint8_t node_id,
                        std::string name = "CAN to PWM", uint32_t sw_version = 0, uint8_t node_priority = 10)
      : Base(name, sw_version, hw_version, node_id, node_priority)
      , sub_pwm_servo(Base::m_ownship_node)
      , sub_pwm_motor(Base::m_ownship_node)
      , m_is_servo(is_servo)
      , m_actuator_id(actuator_id)
  {
  }

  virtual ~Can_to_pwm_translator() override = default;

  virtual void handle_servo_set(const ReceivedDataStructure<servo_handler_type_t> &data) = 0;

  virtual void handle_motor_set(const ReceivedDataStructure<motor_handler_type_t> &data) = 0;

  // TODO: After implementing echo command we must handle it
  virtual void handle_read_pwm() = 0;

  void custom_initialization() override
  {
    if (m_is_servo)
    {
      std::function<void(const ReceivedDataStructure<servo_handler_type_t> &)> servo_func = [this](const auto &data)
      { this->handle_servo_set(data); };
      sub_pwm_servo.start(servo_func);
      Base::set_node_priority(20);
    }
    else
    {
      std::function<void(const ReceivedDataStructure<motor_handler_type_t> &)> motor_func = [this](const auto &data)
      { this->handle_motor_set(data); };
      sub_pwm_motor.start(motor_func);
      Base::set_node_priority(1);
    }
  }

  void custom_broadcast() override
  {
  }

  uint32_t get_actuator_id()
  {
    return m_actuator_id;
  }

  private:
  uavcan::Subscriber<servo_handler_type_t> sub_pwm_servo;
  uavcan::Subscriber<motor_handler_type_t> sub_pwm_motor;

  bool m_is_servo{true};
  const uint32_t m_actuator_id;
};
#pragma GCC diagnostic pop
