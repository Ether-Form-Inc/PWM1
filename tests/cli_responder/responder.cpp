#include <Posix_can_to_pwm.h>
#include <Posix_system_clock.h>
#include <Pwm_commander.h>
#include <usb_interface/Usb_can_interface.h>

#include <cstdint>
#include <iostream>
#include <thread>

// inline constexpr char tty1[] = "/dev/ttyUSB1";
inline constexpr char tty1[] = "/dev/ttyUSB0";
int main()
{
  typedef Posix_can_to_pwm<Usb_can_interface<tty1>, Posix_system_clock> posix_handler_t;
  posix_handler_t c_rcv{0, true, 11};

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  c_rcv.initialize();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  bool stop{false};

  c_rcv.main(stop, 10);
  return 0;
}
