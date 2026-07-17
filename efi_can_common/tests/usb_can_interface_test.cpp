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
  CanFrame frame2;

  /* Set transmit frame */
  uint32_t send_frame_len{8}; // Maximum
  frame1.dlc = CanFrame::dataLengthToDlc(send_frame_len);
  frame1.id = 0xf20a0381;       // Test frame id match across all 4 bytes
  frame1.id &= ((1 << 29) - 1); // Canid is only 29 bits

  for (uint32_t i = 0; i < send_frame_len; i++)
  {
    frame1.data[i] = i;
  }

  /* Setup thread to receive from usbcan*/
  std::atomic<bool> stop{false};
  std::thread receive_thread(
      [&stop, &frame2, &tx_deadline, &can2, &flags, &t]()
      {
        while (!stop && can2.receive(frame2, tx_deadline, t, flags) <= 0)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
      });

  // Give time for thread to initialize then send can frame
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  can1.send(frame1, tx_deadline, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // Join rcv thread
  stop = true;
  receive_thread.join();

  // Test that frame id matches along with all data
  EXPECT_EQ(frame1.id, frame2.id);
  for (uint32_t i = 0; i < send_frame_len; i++)
  {
    EXPECT_EQ(frame1.data[i], frame2.data[i]);
  }
}
