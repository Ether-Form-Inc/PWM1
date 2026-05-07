#include <controllers/pwm_controller.h>
#include <can_interfaces/can_interface.h>

void PWM_controller::write(CAN_interface &interface, uint8_t output_port, uint16_t pulse_width)
{
  uint32_t priority{0};
  uint32_t device_id{};
  etherform_Pwm pwm{.output_port = output_port, .pulse_width = pulse_width};
  interface.write(reinterpret_cast<uint8_t *>(&pwm), priority, ETHERFORM_PWM_ID, device_id, sizeof(etherform_Pwm));
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
