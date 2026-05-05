#pragma once

#include <can_interface.h>

class Usb_can_interface : public CAN_interface
{
  public:
  void read(uint8_t *p_data, uint32_t len) override;
  void write(uint8_t *p_data, uint32_t len) override;
};
