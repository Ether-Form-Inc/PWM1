#include <Can_to_pwm_translator.h>

#include <chrono>
#include <iostream>
#include <thread>

using namespace uavcan;

template <typename CAN_IF, typename SYS_CLK> class Posix_can_to_pwm : public Can_to_pwm_translator<CAN_IF, SYS_CLK>
{
  typedef uavcan::equipment::actuator::ArrayCommand servo_handler_type_t;
  typedef uavcan::equipment::esc::RawCommand motor_handler_type_t;

  typedef Can_to_pwm_translator<CAN_IF, SYS_CLK> Base;

  public:
  Posix_can_to_pwm(uint32_t actuator_id, bool is_servo, uint8_t node_id)
      : Base(actuator_id, is_servo, 0, node_id)
  {
  }
  void handle_servo_set(const ReceivedDataStructure<servo_handler_type_t> &data) override
  {
    auto cmd = data.commands[0];
    std::cout << "Posix servo handle called." << std::endl;
    if (cmd.actuator_id == Base::get_actuator_id())
    {
      std::cout << "Setting servo 0 to: " << cmd.command_value;
      pwm_val = (int32_t)cmd.command_value;
    }
    else
    {
      std::cout << "Actuator id was: " << cmd.actuator_id << " ignored." << std::endl;
    }
  }

  void handle_motor_set(const ReceivedDataStructure<motor_handler_type_t> &data) override
  {
    std::cout << "Posix motor handle called" << std::endl;
  }

  void handle_read_pwm() override
  {
  }

  void delay(uint64_t delay_us) override
  {
    std::this_thread::sleep_for(std::chrono::microseconds(delay_us));
  }

  private:
  int32_t pwm_val{-1};
};
