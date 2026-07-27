#include <gtest/gtest.h>
#include <thread>

#include <usb_interface/Usb_can_interface.h>

#include <Posix_can_interface.h>
#include <Posix_can_to_pwm.h>

constexpr const char *file_descriptor{"/dev/ttyUSB1"};

TEST(PWM_controller, Read)
{
  Posix_can_to_pwm<Posix_can_interface, Posix_system_clock> c_hand{0, true, 10};
  c_hand.initialize();
  c_hand.queue_receive();
  c_hand.do_broadcast();
}
