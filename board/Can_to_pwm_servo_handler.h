#pragma once
#include <etherform/Pwm.hpp>
#include <uavcan/data_type.hpp>
#include <uavcan/node/node.hpp>
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
class Can_to_pwm_servo_handler
{
  typedef etherform::Pwm servo_handler_type_t;
  typedef etherform::Pwm motor_handler_type_t;

  public:
  Can_to_pwm_servo_handler(const bool is_servo);
  ~Can_to_pwm_servo_handler();

  void handle_servo(const ReceivedDataStructure<servo_handler_type_t> &data);

  void handle_motor(const ReceivedDataStructure<motor_handler_type_t> &data);

  void init();

  void queue_receive();

  private:
  bool m_is_servo{true};
  Dummy_can_driver dummy_driver{};
  Dummy_system_clock dummy_clock{};

  // Size = num_can_if * ( sum of all subscriptions{num_pub_senders * sizeof(rcvd_serialized_pub_type)} + sum of all
  // pubs{3 * sizeof(send_serialied_pub_type)} ) (On order of 3000
  uavcan::Node<1024> ownship_node{dummy_driver, dummy_clock}; // Round up to mults of 64

  uavcan::Subscriber<etherform::Pwm> sub_pwm_servo;
  uavcan::Subscriber<etherform::Pwm> sub_pwm_motor;
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
