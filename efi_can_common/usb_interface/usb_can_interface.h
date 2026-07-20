#pragma once

#include <iostream>

#include <uavcan/driver/can.hpp>
#include <uavcan/time.hpp>

#include <usb_interface/usbcan.h>

#include <dummy_can_interface.h>
using namespace uavcan;

template <const char *tty_device, typename SYS_CLK = Dummy_system_clock> class Usb_can_interface : public ICanIface
{

  SYS_CLK sys_clk;

  public:
  Usb_can_interface()
      : m_tty_fd(usbcan::adapter_init(tty_device, CANUSB_TTY_BAUD_RATE_DEFAULT))
  {
    usbcan::command_settings(m_tty_fd, CANUSB_SPEED::CANUSB_SPEED_10000, CANUSB_MODE::CANUSB_MODE_NORMAL);
  }
  ~Usb_can_interface() override
  {
    if (m_tty_fd > 0)
    {
      close(m_tty_fd);
    }
    else
    {
      std::cerr << "Failed to close" << std::endl;
    }
  }

  int16_t receive(CanFrame &out_frame, MonotonicTime &out_ts_monotonic, UtcTime &out_ts_utc,
                  CanIOFlags &out_flags) override
  {
    out_ts_monotonic = sys_clk.getMonotonic();
    out_ts_utc = sys_clk.getUtc();

    out_flags = 0;

    const uint32_t max_len{8};
    uint8_t *p_data{out_frame.data};
    bool is_extended{};

    int ret = usbcan::receive_data_frame(m_tty_fd, p_data, max_len, out_frame.id, is_extended);
    out_frame.dlc = CanFrame::dataLengthToDlc(ret);

    if (is_extended)
    {
      out_frame.id |= CanFrame::FlagEFF;
    }

    return ret;
  }

  int16_t send(const CanFrame &frame, MonotonicTime tx_deadline, CanIOFlags flags) override
  {
    const uint8_t *p_data{frame.data};
    uint32_t can_id{frame.id};
    uint32_t len{(uint32_t)CanFrame::dlcToDataLength(frame.dlc)};

    return usbcan::send_data_frame(m_tty_fd, can_id & CanFrame::MaskExtID, p_data, len);
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

  private:
  const int m_tty_fd;
};
