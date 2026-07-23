#include <Posix_can_to_pwm.h>
#include <Pwm_commander.h>
#include <dummy_can_interface.h> // TODO: Seperate system clock
#include <usb_interface/usb_can_interface.h>

#include <cstdint>
#include <iostream>
#include <thread>

inline constexpr char tty0[] = "/dev/ttyUSB0";
inline constexpr char tty1[] = "/dev/ttyUSB1";
int main()
{
  // TODO: Make this usbcan not dummy iface
  typedef Pwm_commander<Usb_can_interface<tty0>, Dummy_system_clock> Command_t;
  Command_t c_cmd{true, 1, 13};

  typedef Posix_can_to_pwm<Usb_can_interface<tty1>, Dummy_system_clock> posix_handler_t;
  posix_handler_t c_rcv{0, true, 11};

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  c_cmd.initialize();
  c_rcv.initialize();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::thread t(&posix_handler_t::main, &c_rcv);

  std::cout << "Etherform CLI CAN-PWM interface. Copyright 2026." << std::endl;
  std::cout << "Servo [0] or Motor [1]? Please remove props before motor before use." << std::endl;
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
        Command_t::Servo_def servo;
        servo.actuator_id = 0;
        servo.pwm_val = servo_val;
        c_cmd.command_servo_pwm(&servo, 1);
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
