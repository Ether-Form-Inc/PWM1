#pragma once

#include <dronecan_msgs.h>

#include <cstdint>

class CAN_interface;

class PWM_controller
{
  public:
  /// @brief
  /// @param
  /// @param output_port
  /// @param pulse_width
  void write(CAN_interface &interface, uint8_t output_port, uint16_t pulse_width);

  /// TODO: I think that this should probably not be a blocking call?
  /// @brief
  /// @param interface
  /// @return
  etherform_Pwm read(CAN_interface &interface);

  /// @brief
  bool echo(CAN_interface &interface);
};
