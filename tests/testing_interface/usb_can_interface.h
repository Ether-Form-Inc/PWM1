#pragma once

// #include <testing_interface/can_interface.h>
#include <uavcan/driver/can.hpp>
#include <uavcan/time.hpp>

#include <testing_interface/usbcan.h>
using namespace uavcan;

template <const char *tty_device> class Usb_can_interface : public ICanIface
{
  public:
  Usb_can_interface()
      : m_tty_fd(adapter_init(tty_device, CANUSB_TTY_BAUD_RATE_DEFAULT))
  {
    /// TODO: Verify these settings
    command_settings(m_tty_fd, CANUSB_SPEED::CANUSB_SPEED_10000, CANUSB_MODE::CANUSB_MODE_NORMAL,
                     CANUSB_FRAME::CANUSB_FRAME_EXTENDED);
  }
  ~Usb_can_interface() override
  {
    if (m_tty_fd > 0)
    {
      close(m_tty_fd);
    }
  }

  int16_t receive(CanFrame &out_frame, MonotonicTime &out_ts_monotonic, UtcTime &out_ts_utc,
                  CanIOFlags &out_flags) override
  {
    out_flags = 0;

    // TODO: Handle flags and time
    uint32_t len{120}; // (8 bytes data + 4 bytes header?)
    uint8_t *p_data{out_frame.data};

    return 0;
    // return frame_recv(m_tty_fd, reinterpret_cast<unsigned char *>(p_data), len);
  }

  int16_t send(const CanFrame &frame, MonotonicTime tx_deadline, CanIOFlags flags) override
  {
    const uint8_t *p_data{frame.data};
    uint32_t can_id{frame.id};
    uint32_t len{(uint32_t)CanFrame::dlcToDataLength(frame.dlc)}; // (8 bytes data + 4 bytes header?)

    return send_data_frame(m_tty_fd, CANUSB_FRAME_EXTENDED, can_id, p_data, len);
    //   TODO: Handle flags and time
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
