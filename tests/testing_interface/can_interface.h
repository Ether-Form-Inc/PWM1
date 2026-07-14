#pragma once

#include <cstdint>

class CAN_interface
{
  public:
  virtual bool read(uint8_t *p_data, uint32_t len) = 0;
  virtual void write(uint32_t can_id, uint8_t *p_data, uint32_t len) = 0;
};
