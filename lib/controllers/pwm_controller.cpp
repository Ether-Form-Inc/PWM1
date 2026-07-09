#include <can_interfaces/can_interface.h>
#include <can_interfaces/uavcan_serialization.h>
#include <controllers/pwm_controller.h>

#include <etherform/Pwm.hpp>
#include <uavcan/transport/transfer_buffer.hpp>

#include <iostream>

void PWM_controller::write(CAN_interface &interface, uint32_t can_id, uint8_t output_port, uint16_t pulse_width)
{

  etherform::Pwm pwm;
  pwm.output_port = 17;
  pwm.pulse_width = 100;

  uavcan::CanFrame can_frame;
  uavcan::TransferID tid;
  const uavcan::NodeID UAVCAN_NODE_ID(1);

  uavcan_serialization::serialize(can_frame, pwm, tid, uavcan::TransferType::TransferTypeMessageBroadcast,
                                  UAVCAN_NODE_ID, uavcan::NodeID::Broadcast);

  // uint8_t buffer[64];

  // uavcan::StaticTransferBufferImpl transfer_buffer(buffer, sizeof(buffer));
  // uavcan::BitStream bitstream(transfer_buffer);

  //// UAVCAN serialization uses a bit-level serializer
  // uavcan::ScalarCodec codec(bitstream);

  // etherform::Pwm::encode(pwm, codec);

  // for (uint32_t i = 0; i < 20; i++)
  //{
  //   std::cout << (int)buffer[i] << std::endl;
  // }

  // uavcan::CanFrame f(0, (uint8_t *)&buffer, 10);
  uavcan::CanFrame cf;
  uavcan::CanFrame cf2;

  // uavcan::Frame f(etherform::Pwm::DefaultDataTypeID, uavcan::TransferType::TransferTypeMessageBroadcast,
  // UAVCAN_NODE_ID,
  //                 uavcan::NodeID::Broadcast, tid);

  // f.compile(cf);

  // uavcan::Frame f2;
  // f2.parse(cf);
  // f2.compile(cf2);

  // TODO: Set priority (frame.setPriority(TransferPriority))

  std::cout << "ID: " << can_frame.id << std::endl;
  std::cout << "TID: " << (int)f.getTransferID().get() << " " << (int)f2.getTransferID().get() << std::endl;

  // std::cout << "Valid: " << f2.isValid() << std::endl;
  // std::cout << "Ids: " << cf2.id << " " << cf.id << std::endl;
  // std::cout << "TID: " << (int)f.getTransferID().get() << " " << (int)f2.getTransferID().get() << std::endl;
  // std::cout << "Is_broadcast: " << f.getDstNodeID().isBroadcast() << " " << f.getDstNodeID().isBroadcast() <<
  // std::endl; std::cout << "Data id: " << etherform::Pwm::DefaultDataTypeID << " " << f.getDataTypeID().get() <<
  // std::endl;

  //  etherform_Pwm pwm{.output_port = output_port, .pulse_width = pulse_width};
  //  interface.write(can_id, reinterpret_cast<uint8_t *>(&pwm), sizeof(etherform_Pwm));
}

// etherform_Pwm PWM_controller::read(CAN_interface &interface)
//{
//   etherform_Pwm pwm;
//   interface.read(reinterpret_cast<uint8_t *>(&pwm), sizeof(etherform_Pwm));
//   return pwm;
// }

bool PWM_controller::echo(CAN_interface &interface)
{
  return true;
}
