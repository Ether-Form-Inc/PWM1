#include <gtest/gtest.h>

#include <controllers/pwm_controller.h>
#include <can_interfaces/usb_can_interface.h>

constexpr const char *file_descriptor{"/dev/ttyUSB1"};

// TEST(PWM_controller, Write)
// {
//   PWM_controller controller;
//   Usb_can_interface interface{file_descriptor};

//   uint8_t priority{0};
//   uint16_t message_id{20500};
//   uint8_t device_id{1};

//   uint32_t can_id{interface.create_can_id(0, 20500, 1)};

//   uint8_t output_port{0};
//   uint16_t pulse_width{500};

//   controller.write(interface, can_id, output_port, pulse_width);
// }

TEST(PWM_controller, Read)
{
  PWM_controller controller;
  Usb_can_interface interface{file_descriptor};

  uint8_t priority{0};
  uint16_t message_id{20500};
  uint8_t device_id{1};

  uint32_t can_id{interface.create_can_id(0, 20500, 1)};

  uint8_t output_port{0};
  uint16_t pulse_width{500};

  controller.write(interface, can_id, output_port, pulse_width);

  etherform_Pwm pwm{controller.read(interface)};
  ASSERT_EQ(pwm.pulse_width, pulse_width);
}

TEST(PWM_controller, Echo)
{
  PWM_controller controller;
  Usb_can_interface interface{file_descriptor};

  // ASSERT_TRUE(controller.echo(interface));
}
