#include <controllers/time_of_flight_controller.h>
#include <can_interfaces/can_interface.h>
#include <cstring>

void Time_of_Flight_contoller::write(CAN_interface &interface, uint32_t can_id, float distance)
{
  etherform_Time_of_flight tof{.distance = distance};
  interface.write(can_id, reinterpret_cast<uint8_t *>(&tof), sizeof(etherform_Time_of_flight));
}

etherform_Time_of_flight Time_of_Flight_contoller::read(CAN_interface &interface)
{
  etherform_Time_of_flight tof;
  interface.read(reinterpret_cast<uint8_t *>(&tof), sizeof(etherform_Time_of_flight));
  return tof;
}

bool Time_of_Flight_contoller::echo(CAN_interface &interface)
{
  return true;
}
