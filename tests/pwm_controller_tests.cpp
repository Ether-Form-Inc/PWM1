#include <gtest/gtest.h>

#include <controllers/pwm_controller.h>
#include <can_interfaces/usb_can_interface.h>

constexpr const char *file_descriptor{"/dev/null"};

TEST(PWM_controller, Write)
{
  PWM_controller controller;
  Usb_can_interface interface{file_descriptor};

  // controller.write(interface, 0, 0);
}

TEST(PWM_controller, Read)
{
  PWM_controller controller;
  Usb_can_interface interface{file_descriptor};

  // etherform_Pwm{controller.read(interface)};
}

TEST(PWM_controller, Echo)
{
  PWM_controller controller;
  Usb_can_interface interface{file_descriptor};

  // ASSERT_TRUE(controller.echo(interface));
}
