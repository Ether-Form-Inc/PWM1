#include <Posix_can_to_pwm.h>
#include <Pwm_commander.h>
#include <dummy_can_interface.h> // TODO: Seperate system clock
#include <usb_interface/usb_can_interface.h>

#include <cstdint>
#include <iostream>
#include <thread>

inline constexpr char tty1[] = "/dev/ttyUSB1";
int main()
{
  // TODO: Make this usbcan not dummy iface
  typedef Posix_can_to_pwm<Usb_can_interface<tty1>, Dummy_system_clock> posix_handler_t;
  posix_handler_t c_rcv{0, true, 11};

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  c_rcv.initialize();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  c_rcv.main();
  return 0;
}
