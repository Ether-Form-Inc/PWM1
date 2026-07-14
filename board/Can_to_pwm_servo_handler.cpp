#include <Can_to_pwm_servo_handler.h>

#include <iostream>

Can_to_pwm_servo_handler::Can_to_pwm_servo_handler(const bool is_servo)
    : m_is_servo(is_servo)
    , sub_pwm_servo(ownship_node)
    , sub_pwm_motor(ownship_node)
{
}

Can_to_pwm_servo_handler::~Can_to_pwm_servo_handler()
{
}

void Can_to_pwm_servo_handler::handle_servo(const ReceivedDataStructure<servo_handler_type_t> &data)
{
}

void Can_to_pwm_servo_handler::handle_motor(const ReceivedDataStructure<motor_handler_type_t> &data)
{
}

void Can_to_pwm_servo_handler::init()
{
  if (m_is_servo)
  {
    std::function<void(const ReceivedDataStructure<servo_handler_type_t> &)> servo_func = [this](const auto &data)
    { this->handle_servo(data); };
    sub_pwm_servo.start(servo_func);
  }
}

void Can_to_pwm_servo_handler::queue_receive()
{
  ownship_node.spinOnce();
}
