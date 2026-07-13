#include <etherform/Pwm.hpp>
#include <uavcan/data_type.hpp>
#include <uavcan/node/subscriber.hpp>
#include <uavcan/transport/perf_counter.hpp>
#include <uavcan/transport/transfer_listener.hpp>

#include "dummy_can_interface.h"

#include <string>

/*
enum DataTypeKind
{
    DataTypeKindService,
    DataTypeKindMessage
};
*/

// This class defines how to handle can to pwm messages
//// TODO: Abstract this handler to an "efi_board_handler"
class Can_to_pwm_servo_handler : public uavcan::TransferListener
{
  typedef etherform::Pwm handler_type_t;

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
  uavcan::DataTypeDescriptor dtd{uavcan::DataTypeKindMessage, handler_type_t::DefaultDataTypeID, sig,
                                 str_type_name.c_str()};

  Dummy_can_driver dummy_driver;
  // const
  // const uavcan::NodeID UAVCAN_NODE_ID{10}; // TODO: Some mechanism to set this

  // uavcan::Subscriber<etherform::Pwm> sub_pwm_servo;
  // uavcan::Subscriber<etherform::Pwm> sub_pwm_motor;
};

/// TODO:
/// Define this as a subscriber where we do: uavcan::Subscriber<etherform::Pwm, function_handle>
///
/// Actually we can just do uavcan::Subscriber<etherform::Pwm> and it will by default do:
// typename Callback_ = std::function<void (const ReceivedDataStructure<DataType_>&)>
//
//
// Then subscriber obj calls start(callback)
// And then the callback received REceivedDataStructure<ethetform::Pwm>
// Which I think has member type of Pwm vals in addition to canid shit
//
//
// So what we should have is: Obj which defines initialization of this. And hosts callback defn.
// Then we also need object to have can interface, dispatcher, and (publisher or Transfer_sender)
