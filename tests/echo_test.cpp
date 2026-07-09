#include <gtest/gtest.h>

#include <controllers/pwm_controller.h>
// #include <controllers/gps_controller.h>
// #include <controllers/power_controller.h>
// #include <controllers/time_of_flight_controller.h>
#include <can_interfaces/drone_can.h>
#include <can_interfaces/usb_can_interface.h>

constexpr const char *file_descriptor{"/dev/ttyUSB1"};

bool starts_with(const std::string &main_str, const std::string &prefix)
{
  return main_str.rfind(prefix, 0) == 0;
}

bool ends_with(const std::string &main_str, const std::string &suffix)
{
  if (main_str.length() < suffix.length())
    return false;
  return main_str.compare(main_str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

TEST(Echo_test, echo)
{
  std::string message{"Hello"};
  Usb_can_interface interface{file_descriptor};
  interface.write(20000, reinterpret_cast<uint8_t *>(message.data()), 5);
  std::cout << "Sent: " << message << "\n";

  // char response[50];
  // EXPECT_TRUE(interface.read(reinterpret_cast<uint8_t *>(response), 50));

  // std::string response_str{response};
  // ASSERT_TRUE(starts_with(response_str, "Hi from "));
  // ASSERT_TRUE(ends_with(response_str, " board"));
  // std::cout << "Response: " << response_str;
}
