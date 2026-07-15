#include <testing_interface/usb_can_interface.h>
#include <testing_interface/usbcan.h>

Usb_can_interface::Usb_can_interface(const char *tty_device)
    : m_tty_fd(adapter_init(tty_device, CANUSB_TTY_BAUD_RATE_DEFAULT))
{
  /// TODO: Verify these settings
  command_settings(m_tty_fd, CANUSB_SPEED::CANUSB_SPEED_10000, CANUSB_MODE::CANUSB_MODE_NORMAL,
                   CANUSB_FRAME::CANUSB_FRAME_EXTENDED);
}

Usb_can_interface::~Usb_can_interface()
{
  if (m_tty_fd > 0)
  {
    close(m_tty_fd);
  }
}

int16_t Usb_can_interface::receive(CanFrame &out_frame, MonotonicTime &out_ts_monotonic, UtcTime &out_ts_utc,
                                   CanIOFlags &out_flags)
// bool Usb_can_interface::read(uint8_t *p_data, uint32_t len)
{
  // TODO: Handle flags and time
  uint32_t len{12}; // (8 bytes data + 4 bytes header?)
  uint8_t *p_data{out_frame.data};

  return frame_recv(m_tty_fd, reinterpret_cast<unsigned char *>(p_data), len);
  // if (frame_recv(m_tty_fd, reinterpret_cast<unsigned char *>(p_data), len) >= 0)
  //{
  //   return true;
  // }
  // return false;
}

int16_t Usb_can_interface::send(const CanFrame &frame, MonotonicTime tx_deadline, CanIOFlags flags)
// void Usb_can_interface::write(uint32_t can_id, uint8_t *p_data, uint32_t len)
{
  const uint8_t *p_data{frame.data};
  uint32_t can_id{frame.id};
  uint32_t len{(uint32_t)CanFrame::dlcToDataLength(frame.dlc)}; // (8 bytes data + 4 bytes header?)
  // TODO: Handle flags and time
  send_isotp(m_tty_fd, static_cast<unsigned long>(can_id), p_data, len);
}
