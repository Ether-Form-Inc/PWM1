#pragma once

#include <dronecan_msgs.h>

#include <cstdint>

class CAN_interface;

class GPS_controller
{
  public:
  void write(CAN_interface &interface, uint32_t can_id, float (&location)[3], bool locked, uint8_t num_satellites);

  etherform_GPS_Receiver read(CAN_interface &interface);

  /// @brief
  bool echo(CAN_interface &interface);
};
