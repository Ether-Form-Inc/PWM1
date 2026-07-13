// This is just a dummy interface which should verify everything works at top level. We can then sub in usb-can or
// stm-can

#include <uavcan/driver/can.hpp>
#include <uavcan/time.hpp>

#include <iostream>

using namespace uavcan;

class Dummy_can_iface : public ICanIface
{
  public:
  Dummy_can_iface() = default;
  ~Dummy_can_iface() override = default;

  int16_t send(const CanFrame &frame, MonotonicTime tx_deadline, CanIOFlags flags) override
  {
    std::cout << "Sent" << std::endl;
  }

  int16_t receive(CanFrame &out_frame, MonotonicTime &out_ts_monotonic, UtcTime &out_ts_utc,
                  CanIOFlags &out_flags) override
  {
    std::cout << "Received" << std::endl;
  }

  int16_t configureFilters(const CanFilterConfig *filter_configs, uint16_t num_configs) override
  {
    return -1;
  }

  uint16_t getNumFilters() const override
  {
    return 0;
  }

  uint64_t errors{};
  uint64_t getErrorCount() const override
  {
    return errors;
  }
};

class Dummy_can_driver : public ICanDriver
{
  Dummy_can_iface can_iface;

  public:
  Dummy_can_driver() = default;
  ~Dummy_can_driver() override = default;

  ICanIface *getIface(uint8_t iface_index) override
  {
    return &can_iface;
  }

  uint8_t getNumIfaces() const override
  {
    return 1;
  }

  int16_t select(CanSelectMasks &inout_masks, const CanFrame *(&pending_tx)[MaxCanIfaces],
                 MonotonicTime blocking_deadline) override
  {
    return 1;
  }
};
