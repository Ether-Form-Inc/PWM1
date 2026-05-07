#include <gtest/gtest.h>

#include <can_interfaces/usb_can_interface.h>

constexpr const char *file_descriptor{"/dev/null"};

TEST(Usb_can_interface, Create_can_id)
{
  Usb_can_interface interface{file_descriptor};

  uint32_t priority{5};
  uint32_t message_id{20000};
  uint8_t device_id{15};

  uint32_t can_id{interface.create_can_id(priority, message_id, device_id)};

  ASSERT_EQ(can_id, 0b00000101010011100010000000001111);
}
