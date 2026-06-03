#pragma once

#include <dronecan_msgs.h>

#include <cstdint>

class CAN_interface;

class Time_of_Flight_contoller
{
  public:
  void write(CAN_interface &interface, uint32_t can_id, float distance);

  etherform_Time_of_flight read(CAN_interface &interface);

  /// @brief
  bool echo(CAN_interface &interface);
};
