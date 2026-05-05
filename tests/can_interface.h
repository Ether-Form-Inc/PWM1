#pragma once

#include <cstdint>

class CAN_interface
{
  public:
  virtual void read(uint8_t *p_data, uint32_t len) = 0;
  virtual void write(uint8_t *p_data, uint32_t len) = 0;
};
