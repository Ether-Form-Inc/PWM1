#pragma once

#include <cstdint>

class Drone_can
{
  public:
  static uint32_t create_id(uint8_t priority, uint16_t message_id, uint8_t device_id);
  static uint32_t get_message_id(uint8_t *p_can_frame);
};
