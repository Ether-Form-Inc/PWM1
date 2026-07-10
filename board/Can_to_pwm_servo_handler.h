#include <etherform/Pwm.hpp>
#include <uavcan/data_type.hpp>
#include <uavcan/transport/perf_counter.hpp>
#include <uavcan/transport/transfer_listener.hpp>

#include <string>

/*
enum DataTypeKind
{
    DataTypeKindService,
    DataTypeKindMessage
};
*/

// This class defines how to handle can to pwm messages
class Can_to_pwm_servo_handler : public uavcan::TransferListener
{
  public:
  Can_to_pwm_servo_handler(const bool is_servo);
  ~Can_to_pwm_servo_handler();

  virtual void handleIncomingTransfer(uavcan::IncomingTransfer &transfer) override;

  private:
  static constexpr uint16_t max_buffs{100};
  uavcan::PoolAllocator<10 * max_buffs, max_buffs> pool_allocator;
  uavcan::TransferPerfCounter perf;
  const uavcan::DataTypeSignature sig;
  const std::string str_type_name{"Servo_handler"};
  uavcan::DataTypeDescriptor dtd{uavcan::DataTypeKindMessage, etherform::Pwm::DefaultDataTypeID, sig,
                                 str_type_name.c_str()};
};
