#pragma once

#include <dronecan_msgs.h>

#include <cstdint>

class CAN_interface;

class PWM_controller
{
  public:
  void write(CAN_interface &interface, uint32_t can_id, uint8_t output_port, uint16_t pulse_width);

  /// TODO: I think that this should probably not be a blocking call?
  etherform_Pwm read(CAN_interface &interface);

  /// @brief
  bool echo(CAN_interface &interface);
};
