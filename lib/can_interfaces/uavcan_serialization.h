#include <etherform/Pwm.hpp>
#include <uavcan/transport/transfer_buffer.hpp>
#include <uavcan/transport/transfer_receiver.hpp>

#include <iostream>

// enum TransferType
//{
//     TransferTypeServiceResponse  = 0,
//     TransferTypeServiceRequest   = 1,
//     TransferTypeMessageBroadcast = 2
// };
// TODO: Make compiler def with ownship node ID defined rather than passing in src node id
namespace uavcan_serialization
{
  template <typename MSG_TYPE>
  void serialize(uavcan::CanFrame &can_frame, const MSG_TYPE &msg, uavcan::TransferID &tid,
                 const uavcan::TransferType type, const uavcan::NodeID &src, const uavcan::NodeID &dst)
  {
    // Serialize payload
    uint8_t buffer[64];

    uavcan::StaticTransferBufferImpl transfer_buffer(buffer, sizeof(buffer));
    uavcan::BitStream bitstream(transfer_buffer);
    uavcan::ScalarCodec codec(bitstream);

    MSG_TYPE::encode(msg, codec);

    // Encode frame with msg payload and canid
    uavcan::Frame frame(MSG_TYPE::DefaultDataTypeID, type, src, dst, tid);

    const uint32_t payload_size_bytes{(MSG_TYPE::MaxBitLen + 1) / 8};

    frame.setStartOfTransfer(true);
    frame.setEndOfTransfer(true);
    frame.setPayload(buffer, payload_size_bytes); // Omit tail byte

    // Increment transfer id
    tid.increment();

    // Compile frame into can_frame
    //// TODO: Vector of can frames?
    frame.compile(can_frame);

    constexpr uint16_t max_buffs{100};
    uavcan::PoolAllocator<10 * max_buffs, max_buffs> pool_allocator;
    // uavcan::TransferBufferManagerKey tbmk(UAVCAN_NODE_ID, uavcan::TransferType::TransferTypeMessageBroadcast);
    uavcan::TransferBufferManagerKey tbmk(src, uavcan::TransferType::TransferTypeMessageBroadcast);
    uavcan::TransferBufferManager tbm(max_buffs, pool_allocator);
    // tbm.create(tbmk);

    uavcan::TransferBufferAccessor tba(tbm, tbmk);

    uavcan::TransferReceiver tr;

    uavcan::MonotonicTime timestamp = uavcan::MonotonicTime::fromUSec(1);
    uavcan::UtcTime ts_utc; //(0);

    // tba.create();
    tr.addFrame(uavcan::RxFrame(frame, timestamp, ts_utc, 1), tba);

    uavcan::Frame frame2(MSG_TYPE::DefaultDataTypeID, type, src, dst, tid);
    uavcan::MonotonicTime timestamp2 = uavcan::MonotonicTime::fromUSec(2);

    frame2.setStartOfTransfer(false);
    frame2.setEndOfTransfer(true);
    frame2.setPayload(buffer, payload_size_bytes); // Omit tail byte
    std::cout << "SIZEOF MESG: " << sizeof(MSG_TYPE) << std::endl;
    tr.addFrame(uavcan::RxFrame(frame2, timestamp2, ts_utc, 1), tba);

    const uint8_t *set_buf = frame2.getPayloadPtr();

    uint8_t buf[10];
    // For single frame case we use the frame ptr directly
    // TODO: This is for the receive flow!
    auto p = tba.access();
    if (p)
    {
      p->read(0, buf, payload_size_bytes + 1);
      std::cout << (int)buf[0] << " " << (int)buf[1] << " " << (int)buf[2] << std::endl;
    }
    else
    {
      std::cout << "buffer was nullptr *********" << std::endl;
    }

    // Transmit flow (serialization)
    // Encode
    // make frame to build CANid?
    // Repeat CANid, seperate message into stuff?
  }

  /// TODO: Send vector of canframes?
  /// Send one can frame and store data somewhere in case we are not finished
  template <typename MSG_TYPE> void deserialize(MSG_TYPE &msg)
  {
  }
};

/*while (!rx_queue.empty())
{
    uavcan::CanFrame cf = rx_queue.pop();

    uavcan::RxFrame rx_frame(cf, timestamp, iface_index);

    auto result = receiver.addFrame(rx_frame);

    if (result == TransferReceiver::ResultComplete)
    {
        const auto& payload = receiver.getPayload();

        uavcan::ScalarCodec codec(
            payload.begin(),
            payload.size());

        my_namespace::MyMessage msg;

        if (msg.decode(codec) > 0)
        {
            handleMessage(msg);
        }
    }
}
 *
 */
