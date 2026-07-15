#include <gtest/gtest.h>
#include <thread>

#include <testing_interface/usb_can_interface.h>

#include <Can_to_pwm_translator.h>
#include <dummy_can_interface.h>

constexpr const char *file_descriptor{"/dev/ttyUSB1"};

TEST(PWM_controller, Read)
{
  Can_to_pwm_translator<Dummy_can_iface, Dummy_system_clock> c_hand{true, 1, 10};
  c_hand.initialize();
  c_hand.queue_receive();
  c_hand.do_broadcast();
}
