#include <Pwm_commander.h>
#include <dummy_can_interface.h>

#include <cstdint>
#include <iostream>

int main()
{
  // TODO: Make this usbcan not dummy iface
  Pwm_commander<Dummy_can_iface, Dummy_system_clock> c_hand{true, 1, 10};
  c_hand.initialize();

  std::cout << "Etherform CLI CAN-PWM interface. Copyright 2026." << std::endl;
  std::cout << "Servo [0] or Motor [1]? Please remove props fro motor before use." << std::endl;
  int input_servo;
  std::cin >> input_servo;

  bool is_servo{input_servo == 0};
  if (is_servo)
  {
    while (1)
    {
      uint32_t servo_val;
      std::cout << "Enter value between 0 and 2000 to command servo 0." << std::endl;
      std::cin >> servo_val;
      if (servo_val > 2000)
      {
        std::cout << "Invalid value, ignoring value." << std::endl;
      }
      else
      {
        c_hand.command_servo_pwm(servo_val);
      }
    }
  }
  else
  {
    while (1)
    {
      uint32_t motor_val;
      std::cout << "Enter value between 0 and 2000 to command motor 1." << std::endl;
      std::cin >> motor_val;
      exit(0);
    }
  }
  return 0;
}
