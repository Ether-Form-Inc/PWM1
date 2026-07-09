
// enum TransferType
//{
//     TransferTypeServiceResponse  = 0,
//     TransferTypeServiceRequest   = 1,
//     TransferTypeMessageBroadcast = 2
// };
// TODO: Make compiler def with ownship node ID defined rather than passing in src node id
namespace uavcan_serialization
{
  // template <typename MSG_TYPE>
  //  void serialize(uavcan::CanFrame &can_frame, const MSG_TYPE &msg, uavcan::TransferID &tid,
  //                 const uavcan::TransferType type, const uavcan::NodeID &src, const uavcan::NodeID &dst)
  //{
  //    // Serialize payload
  //    // uint8_t buffer[64];

  //  // uavcan::StaticTransferBufferImpl transfer_buffer(buffer, sizeof(buffer));
  //  // uavcan::BitStream bitstream(transfer_buffer);
  //  // uavcan::ScalarCodec codec(bitstream);

  //  // MSG_TYPE::encode(msg, codec);

  //  //// Encode frame with msg payload and canid
  //  // uavcan::Frame frame(MSG_TYPE::DefaultDataTypeID, type, src, dst, tid);

  //  //// Increment transfer id
  //  // tid.increment();

  //  //// Compile frame into can_frame
  //  // frame.compile(can_frame);
  //}

  // template <typename MSG_TYPE> void deserialize(MSG_TYPE &msg)
  //{
  // }
};
