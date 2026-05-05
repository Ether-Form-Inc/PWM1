#pragma once

#include <cstdint>

class CAN_interface;

class PWM_controller
{
  public:
  void write(CAN_interface &interface, uint32_t output_port, uint32_t pulse_width);
  void read(CAN_interface &interface, uint32_t &pulse_width);

  bool echo(CAN_interface &interface);
};
