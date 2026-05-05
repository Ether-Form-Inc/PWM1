#include <pwm_controller.h>

#include <can_interface.h>

void PWM_controller::write(CAN_interface &interface, uint32_t output_port, uint32_t pulse_width)
{
  uint8_t data[8]{};
  interface.write(data, sizeof(data));
}

void PWM_controller::read(CAN_interface &interface, uint32_t &pulse_width)
{
  uint8_t data[8]{};
  interface.read(data, sizeof(data));
}

bool PWM_controller::echo(CAN_interface &interface)
{
  return true;
}
