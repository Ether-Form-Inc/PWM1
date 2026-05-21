#include <can_interfaces/drone_can.h>

#include <iostream>

uint32_t Drone_can::create_id(uint8_t priority, uint16_t message_id, uint8_t device_id)
{
  uint32_t can_id{};

  can_id |= (static_cast<uint32_t>(priority) & 0x1F) << 24;
  can_id |= (static_cast<uint32_t>(message_id) & 0xFFFF) << 8;
  can_id &= ~(1 << 7); // Zero out 8th bit
  can_id |= static_cast<uint32_t>(device_id) & 0x7F;

  return can_id;
}

uint32_t Drone_can::get_message_id(uint8_t *p_can_frame)
{
  uint32_t can_id{*reinterpret_cast<uint32_t *>(&p_can_frame[2])};
  uint32_t message_id{(can_id >> 8) & 0xFFFF};
  return message_id;
}
