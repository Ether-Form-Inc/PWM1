#include <gtest/gtest.h>

#include <controllers/pwm_controller.h>
#include <controllers/gps_controller.h>
#include <controllers/power_controller.h>
#include <controllers/time_of_flight_controller.h>
#include <can_interfaces/usb_can_interface.h>
#include <can_interfaces/drone_can.h>

constexpr const char *file_descriptor{"/dev/ttyUSB1"};

TEST(PWM_controller, Read)
{
  PWM_controller controller;
  Usb_can_interface interface{file_descriptor};

  uint8_t priority{0};
  uint16_t message_id{20500};
  uint8_t device_id{1};

  uint32_t can_id{Drone_can::create_id(4, 20500, 1)};

  uint8_t output_port{0};
  uint16_t pulse_width{500};

  controller.write(interface, can_id, output_port, pulse_width);

  // etherform_Pwm pwm{controller.read(interface)};
  // ASSERT_EQ(pwm.pulse_width, pulse_width);
}

TEST(GPS_controller, Read)
{
  GPS_controller controller;
  Usb_can_interface interface{file_descriptor};

  uint32_t can_id{Drone_can::create_id(4, 20600, 1)};

  float location[3]{12.345f, 67.890f, 100.0f};
  bool locked{true};
  uint8_t num_satellites{8};

  controller.write(interface, can_id, location, locked, num_satellites);

  // etherform_GPS_Receiver gps_data{controller.read(interface)};
  // ASSERT_EQ(gps_data.location[0], location[0]);
  // ASSERT_EQ(gps_data.location[1], location[1]);
  // ASSERT_EQ(gps_data.location[2], location[2]);
  // ASSERT_EQ(gps_data.locked, locked);
  // ASSERT_EQ(gps_data.num_satellites, num_satellites);
}

TEST(Power_controller, Read)
{
  Power_controller controller;
  Usb_can_interface interface{file_descriptor};

  uint32_t can_id{Drone_can::create_id(4, 20700, 1)};

  float voltage{12.5f};
  float temperature[4]{25.0f, 30.0f, 35.0f, 40.0f};

  controller.write(interface, can_id, voltage, temperature);

  // etherform_Power power_data{controller.read(interface)};
  // ASSERT_EQ(power_data.voltage, voltage);
  // ASSERT_EQ(power_data.temperature[0], temperature[0]);
  // ASSERT_EQ(power_data.temperature[1], temperature[1]);
  // ASSERT_EQ(power_data.temperature[2], temperature[2]);
  // ASSERT_EQ(power_data.temperature[3], temperature[3]);
}

TEST(Time_of_Flight_controller, Read)
{
  Time_of_Flight_controller controller;
  Usb_can_interface interface{file_descriptor};

  uint32_t can_id{Drone_can::create_id(4, 20800, 1)};

  float distance{150.5f};

  controller.write(interface, can_id, distance);

  // etherform_Time_of_flight tof_data{controller.read(interface)};
  // ASSERT_EQ(tof_data.distance, distance);
}
