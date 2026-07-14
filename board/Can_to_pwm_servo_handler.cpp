#include <Can_to_pwm_servo_handler.h>

#include <iostream>

Can_to_pwm_servo_handler::Can_to_pwm_servo_handler(const bool is_servo)
    : m_is_servo(is_servo)
    , sub_pwm_servo(ownship_node)
    , sub_pwm_motor(ownship_node)
{
  uavcan::protocol::SoftwareVersion sw_version; // Standard type uavcan.protocol.SoftwareVersion
  ownship_node.setNodeID(uavcan::NodeID(10));
  ownship_node.setName("test");
  sw_version.major = 1;
  ownship_node.setSoftwareVersion(sw_version);
  uavcan::protocol::HardwareVersion hw_version;
  hw_version.major = 1;
  ownship_node.setHardwareVersion(hw_version);
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

  int node_start_res;
  if (m_is_servo)
  {
    node_start_res = ownship_node.start({25});
  }
  else
  {
    node_start_res = ownship_node.start({0});
  }

  if (node_start_res < 0)
  {
    std::cerr << "Failed to start the node; error: " + node_start_res << std::endl;
  }
  else
  {
    ownship_node.setModeOperational();
  }
}

void Can_to_pwm_servo_handler::queue_receive()
{
  const int res = ownship_node.spinOnce();
  if (res < 0)
  {
    std::cerr << "Transient failure: " << res << std::endl;
  }
}
