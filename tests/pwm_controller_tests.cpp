#include <gtest/gtest.h>

#include <pwm_controller.h>
#include <usb_can_interface.h>

TEST(PWM_controller, Write)
{
  PWM_controller controller;
  Usb_can_interface interface{"/dev/ttyUSB0"};
  controller.write(interface, 0, 0);
}

TEST(PWM_controller, Read)
{
  PWM_controller controller;
  Usb_can_interface interface{"/dev/ttyUSB0"};

  uint32_t pulse_width{};
  controller.read(interface, pulse_width);
}

TEST(PWM_controller, Echo)
{
  PWM_controller controller;
  Usb_can_interface interface{"/dev/ttyUSB0"};

  ASSERT_TRUE(controller.echo(interface));
}
