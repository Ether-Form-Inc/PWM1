#pragma once

// #include <testing_interface/can_interface.h>
#include <uavcan/driver/can.hpp>
#include <uavcan/time.hpp>

using namespace uavcan;

class Usb_can_interface : public ICanIface
{
  public:
  explicit Usb_can_interface(const char *tty_device);
  ~Usb_can_interface();

  int16_t receive(CanFrame &out_frame, MonotonicTime &out_ts_monotonic, UtcTime &out_ts_utc,
                  CanIOFlags &out_flags) override;

  int16_t send(const CanFrame &frame, MonotonicTime tx_deadline, CanIOFlags flags) override;

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

  private:
  const int m_tty_fd;
};
