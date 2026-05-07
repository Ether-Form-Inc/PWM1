#pragma once

#include <cstdint>

class CAN_interface
{
  public:
  virtual void read(uint8_t *p_data, uint32_t len) = 0;
  virtual void write(uint8_t *p_data, uint32_t priority, uint32_t message_id, uint8_t device_id, uint32_t len) = 0;
};
