#include <Pwm_commander.h>
#include <usb_interface/Usb_can_interface.h>

#include <gtest/gtest.h>
#include <thread>

inline constexpr char tty0[] = "/dev/ttyUSB0";

TEST(PWM_controller, echo)
{
  std::cout << "This test requires that a uavcan device is connected to a UAVCAN compatible device." << std::endl;
  std::cout << "Please enter the nodeid of the uavcan device" << std::endl;
  uint32_t node_of_interest;
  std::cin >> node_of_interest;

  typedef Pwm_commander<Usb_can_interface<tty0>> Command_t;
  Command_t efi_can1{true, 1, 13, "CAN to PWM", 0, 10, (uint8_t)node_of_interest};
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  efi_can1.initialize();
  bool stop{false};

  // Start thread to constantly receive from object we send request to
  std::thread service_receive_thread(
      [&efi_can1, &stop]()
      {
        while (!stop)
        {
          efi_can1.queue_receive();
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      });

  std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait for receive thread to initalize

  // Send service request
  const uint32_t num_service_requests{3};
  for (uint32_t s = 0; s < num_service_requests; s++)
  {
    efi_can1.do_broadcast();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  stop = true;
  service_receive_thread.join();

  ASSERT_EQ(efi_can1.get_received_echos(), num_service_requests);
}

TEST(PWM_controller, read_pwm)
{
  // This board has GPIO connected and can thus read the PWM signal being output by the PCA chip. Once implemented it
  // can be verified  here.
}
