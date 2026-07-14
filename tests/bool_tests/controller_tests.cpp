#include <gtest/gtest.h>

#include <Can_to_pwm_servo_handler.h>
#include <controllers/pwm_controller.h>
#include <testing_interface/usb_can_interface.h>

constexpr const char *file_descriptor{"/dev/ttyUSB1"};

TEST(PWM_controller, Read)
{
  Can_to_pwm_servo_handler c_hand{true};

  c_hand.init();
  c_hand.queue_receive();
  c_hand.queue_receive();
  c_hand.queue_receive();
}
