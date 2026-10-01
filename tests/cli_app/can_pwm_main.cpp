#include <Pwm_commander.h>
#include <usb_interface/Usb_can_interface.h>
#include <user_defined_threshold.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

inline constexpr char tty0[] = "/dev/ttyUSB0";
int main()
{
  typedef Pwm_commander<Usb_can_interface<tty0>> Command_t;
  Command_t c_cmd{true, 1, 13};

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  c_cmd.initialize();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::cout << "Etherform CLI CAN-PWM interface. Copyright 2026." << std::endl;
  std::cout << "Servo [0] or Motor [1]? Please remove props before motor before use." << std::endl;
  int input_servo;
  std::cin >> input_servo;

  std::cout << "How many simultaneous outputs would you like to update?" << std::endl;
  uint32_t simult_outputs{1};
  std::cin >> simult_outputs;

  bool is_servo{input_servo == 0};
  if (is_servo)
  {
    simult_outputs = std::min(simult_outputs, (uint32_t)15); // Maximum uavcan size actuator
  }
  else
  {
    simult_outputs = std::min(simult_outputs, (uint32_t)20); // Maximum uavcan size for esc
  }

  if (is_servo)
  {
    while (1)
    {
      std::cout << "************************************" << std::endl;
      std::vector<Command_t::Servo_def> servos;
      for (uint32_t c = 0; c < simult_outputs; c++)
      {
        Command_t::Servo_def servo;

        std::cout << "Servo: " << c + 1 << "/" << simult_outputs << std::endl;
        std::cout << "Which actuator would you like to update?" << std::endl;
        std::cin >> servo.actuator_id;

        std::cout << "Enter value between 0 and 2000 to command servo " << servo.actuator_id << std::endl;
        std::cin >> servo.pwm_val;
        if (servo.pwm_val > maximum_servo_us_4)
        {
          std::cout << "Invalid value, ignoring value." << std::endl;
        }
        else
        {
          servos.push_back(servo);
        }
      }
      c_cmd.command_servo_pwm(servos.data(), servos.size());
    }
  }
  else
  {
    while (1)
    {
      std::cout << "************************************" << std::endl;
      std::vector<int16_t> motors;

      for (uint32_t c = 0; c < simult_outputs; c++)
      {
        int16_t motor_val;
        std::cout << "Motor: " << c + 1 << "/" << simult_outputs << std::endl;
        std::cout << "Enter value between -8192 and 8191 to command motor " << c + 1 << "/" << simult_outputs
                  << std::endl;
        std::cin >> motor_val;

        if (motor_val > 8191 || motor_val < -8192)
        {
          std::cout << "Invalid value, ignoring value." << std::endl;
        }
        else
        {
          motors.push_back(motor_val);
        }
      }
      c_cmd.command_motor_pwm(motors.data(), motors.size());
    }
  }
  return 0;
}
