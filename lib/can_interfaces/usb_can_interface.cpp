#include <can_interfaces/usb_can_interface.h>
#include <can_interfaces/usbcan.h>
#include <dronecan_msgs.h>

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

bool Usb_can_interface::read(uint8_t *p_data, uint32_t len)
{
  if (frame_recv(m_tty_fd, reinterpret_cast<unsigned char *>(p_data), len) >= 0)
  {
    return true;
  }
  return false;
}

void Usb_can_interface::write(uint32_t can_id, uint8_t *p_data, uint32_t len)
{
  send_data_frame(m_tty_fd, CANUSB_FRAME::CANUSB_FRAME_EXTENDED, static_cast<unsigned long>(can_id),
                  reinterpret_cast<unsigned char *>(p_data), len);
}
