#include <controllers/power_controller.h>
#include <can_interfaces/can_interface.h>
#include <cstring>

void Power_controller::write(CAN_interface &interface, uint32_t can_id, float voltage, float (&temperature)[4])
{
  etherform_Power power{.voltage = voltage};
  memcpy(power.temperature, temperature, 4 * sizeof(float));
  interface.write(can_id, reinterpret_cast<uint8_t *>(&power), sizeof(etherform_Power));
}

etherform_Power Power_controller::read(CAN_interface &interface)
{
  etherform_Power power;
  interface.read(reinterpret_cast<uint8_t *>(&power), sizeof(etherform_Power));
  return power;
}

bool Power_controller::echo(CAN_interface &interface)
{
  return true;
}
