#include <usb_interface/usb_can_interface.h>

#include <cstdint>
#include <iostream>
#include <thread>

inline constexpr char tty0[] = "/dev/ttyUSB0";
inline constexpr char tty1[] = "/dev/ttyUSB1";

int main()
{
  Usb_can_interface<tty0> can1;
  Usb_can_interface<tty1> can2;

  MonotonicTime tx_deadline;
  UtcTime t;
  CanIOFlags flags;

  CanFrame frame1;
  frame1.id = 100;
  frame1.data[0] = 0;
  frame1.data[1] = 1;
  frame1.data[2] = 2;
  frame1.data[3] = 3;
  can1.send(frame1, tx_deadline, 0);

  CanFrame frame2;
  can2.receive(frame2, tx_deadline, t, flags);

  if (frame2.data[2] == frame1.data[2])
  {
    std::cout << "Possible good test. " << std::endl;
  }
  else
  {
    std::cout << "Failed test" << std::endl;
  }
  return 0;
}
