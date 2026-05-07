#include <controllers/pwm_controller.h>
#include <can_interfaces/can_interface.h>

void PWM_controller::write(CAN_interface &interface, uint32_t can_id, uint8_t output_port, uint16_t pulse_width)
{
  etherform_Pwm pwm{.output_port = output_port, .pulse_width = pulse_width};
  interface.write(can_id, reinterpret_cast<uint8_t *>(&pwm), sizeof(etherform_Pwm));
}

etherform_Pwm PWM_controller::read(CAN_interface &interface)
{
  etherform_Pwm pwm;
  interface.read(reinterpret_cast<uint8_t *>(&pwm), sizeof(etherform_Pwm));
  return pwm;
}

bool PWM_controller::echo(CAN_interface &interface)
{
  return true;
}
