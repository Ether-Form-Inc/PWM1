#include <Can_to_pwm_servo_handler.h>

Can_to_pwm_servo_handler::Can_to_pwm_servo_handler(const bool is_servo)
    : TransferListener(perf, dtd, max_buffs, pool_allocator)
//, sub_pwm_servo(staticUAVCAN_NODE_ID)
//, sub_pwm_motor(UAVCAN_NODE_ID)
{
}

Can_to_pwm_servo_handler::~Can_to_pwm_servo_handler()
{
}

void Can_to_pwm_servo_handler::handleIncomingTransfer(uavcan::IncomingTransfer &transfer)
{
  constexpr uint32_t payload_size_bytes{(handler_type_t::MaxBitLen + 1) / 8};
  uint8_t buffer[payload_size_bytes];
  transfer.read(0, buffer, payload_size_bytes);

  etherform::Pwm *pwm{reinterpret_cast<etherform::Pwm *>(buffer)};
}
