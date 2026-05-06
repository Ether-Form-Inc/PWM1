#include <can_interfaces/usb_can_interface.h>

#include <can_interfaces/usbcan.h>

#include <dronecan_msgs.h>

Usb_can_interface::Usb_can_interface(const char *tty_device)
    : m_tty_fd(adapter_init(tty_device, CANUSB_TTY_BAUD_RATE_DEFAULT))
{
  /// TODO: Verify these settings
  command_settings(m_tty_fd, CANUSB_SPEED::CANUSB_SPEED_10000, CANUSB_MODE::CANUSB_MODE_NORMAL,
                   CANUSB_FRAME::CANUSB_FRAME_STANDARD);
}

void Usb_can_interface::read(uint8_t *p_hex_data, uint32_t len)
{
  const int id{0};
  send_data_frame(m_tty_fd, CANUSB_FRAME::CANUSB_FRAME_STANDARD, id, reinterpret_cast<unsigned char *>(p_hex_data),
                  len);
}

void Usb_can_interface::write(uint8_t *p_data, uint32_t len)
{
  /// TODO: Allegedly there needs to be a write that says a receive is happening and then a sleep before this frame recv
  frame_recv(m_tty_fd, reinterpret_cast<unsigned char *>(p_data), len);
}
