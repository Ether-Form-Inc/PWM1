#include <usb_interface/usb_can_interface.h>

#include <atomic>
#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>

inline constexpr char tty0[] = "/dev/ttyUSB0";
inline constexpr char tty1[] = "/dev/ttyUSB1";

TEST(efi_can_common, usb_can_interface)
{
  Usb_can_interface<tty0> can1;
  Usb_can_interface<tty1> can2;

  MonotonicTime tx_deadline;
  UtcTime t;
  CanIOFlags flags;

  CanFrame frame1;
  frame1.dlc = CanFrame::dataLengthToDlc(4);
  frame1.id = 100;
  frame1.data[0] = 0;
  frame1.data[1] = 1;
  frame1.data[2] = 2;
  frame1.data[3] = 3;

  std::atomic<bool> stop{false};

  CanFrame frame2;

  std::thread thr(
      [&stop, &frame2, &tx_deadline, &can2, &flags, &t]()
      {
        while (!stop && can2.receive(frame2, tx_deadline, t, flags) <= 0)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
      });

  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  can1.send(frame1, tx_deadline, 0);

  stop = true;
  thr.join();

  if (frame2.data[2] == frame1.data[2])
  {
    std::cout << "Possible good test. " << std::endl;
  }
  else
  {
    std::cout << "Failed test " << (int)frame2.data[2] << std::endl;
  }
}
