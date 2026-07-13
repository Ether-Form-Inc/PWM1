#pragma once
// This is just a dummy interface which should verify everything works at top level. We can then sub in usb-can or
// stm-can

#include <uavcan/driver/can.hpp>
#include <uavcan/driver/system_clock.hpp>
#include <uavcan/time.hpp>

#include <chrono>
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
    return 0;
  }

  int16_t receive(CanFrame &out_frame, MonotonicTime &out_ts_monotonic, UtcTime &out_ts_utc,
                  CanIOFlags &out_flags) override
  {
    std::cout << "Received" << std::endl;
    return 0;
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

class Dummy_system_clock : public uavcan::ISystemClock
{
  using Clock = std::chrono::steady_clock;

  const Clock::time_point start_ = Clock::now();

  mutable uavcan::UtcDuration utc_offset_;

  public:
  Dummy_system_clock()
      : utc_offset_(uavcan::UtcDuration::fromUSec(0))
  {
  }

  uavcan::MonotonicTime getMonotonic() const override
  {
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start_).count();

    return uavcan::MonotonicTime::fromUSec(static_cast<uint64_t>(elapsed));
  }

  uavcan::UtcTime getUtc() const override
  {
    return uavcan::UtcTime::fromUSec(getMonotonic().toUSec() + utc_offset_.toUSec());
  }

  void adjustUtc(uavcan::UtcDuration adjustment) override
  {
    utc_offset_ += adjustment;
  }
};
