#include <controllers/gps_controller.h>
#include <can_interfaces/can_interface.h>
#include <cstring>

void GPS_controller::write(CAN_interface &interface, uint32_t can_id, float (&location)[3], bool locked, uint8_t num_satellites)
{
  etherform_GPS_Receiver gps{.location = locked, .num_satellites = num_satellites};
  memcpy(gps.location, location, 3 * sizeof(float));
  interface.write(can_id, reinterpret_cast<uint8_t *>(&gps), sizeof(etherform_GPS_Receiver));
}

etherform_GPS_Receiver GPS_controller::read(CAN_interface &interface)
{
  etherform_GPS_Receiver gps;
  interface.read(reinterpret_cast<uint8_t *>(&gps), sizeof(etherform_GPS_Receiver));
  return gps;
}

bool GPS_controller::echo(CAN_interface &interface)
{
  return true;
}
