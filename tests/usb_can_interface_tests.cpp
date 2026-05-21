#include <gtest/gtest.h>

#include <can_interfaces/drone_can.h>

TEST(Drone_can, Create_id)
{
  uint8_t priority{5};
  uint16_t message_id{20000};
  uint8_t device_id{15};

  uint32_t can_id{Drone_can::create_id(priority, message_id, device_id)};

  ASSERT_EQ(can_id, 0b00000101010011100010000000001111);
}
