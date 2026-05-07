#include <gtest/gtest.h>

#include <can_interfaces/usb_can_interface.h>

constexpr const char *file_descriptor{"/dev/ttyUSB1"};

TEST(Usb_can_interface, Create_can_id)
{
  Usb_can_interface interface{file_descriptor};

  uint8_t priority{5};
  uint16_t message_id{20000};
  uint8_t device_id{15};

  uint32_t can_id{interface.create_can_id(priority, message_id, device_id)};

  ASSERT_EQ(can_id, 0b00000101010011100010000000001111);
}
