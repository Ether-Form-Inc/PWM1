#pragma once

#include <dronecan_msgs.h>

#include <cstdint>

class CAN_interface;

class Power_controller
{
  public:
  void write(CAN_interface &interface, uint32_t can_id, float voltage, float (&temperature)[4]);

  etherform_Power read(CAN_interface &interface);

  /// @brief
  bool echo(CAN_interface &interface);
};
