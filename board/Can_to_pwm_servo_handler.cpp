#include <Can_to_pwm_servo_handler.h>

Can_to_pwm_servo_handler::Can_to_pwm_servo_handler(const bool is_servo)
    : TransferListener(perf, dtd, max_buffs, pool_allocator)
{
}

Can_to_pwm_servo_handler::~Can_to_pwm_servo_handler()
{
}

void Can_to_pwm_servo_handler::handleIncomingTransfer(uavcan::IncomingTransfer &transfer)
{
}
