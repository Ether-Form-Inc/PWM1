#pragma once

#include <dronecan_msgs.h>

#include <cstdint>

class CAN_interface;

class PWM_controller
{
  public:
  void write(CAN_interface &interface, uint32_t can_id, uint8_t output_port, uint16_t pulse_width);

  etherform_Pwm read(CAN_interface &interface);

  /// @brief
  bool echo(CAN_interface &interface);
};
