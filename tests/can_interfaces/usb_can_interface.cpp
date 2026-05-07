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

uint32_t Usb_can_interface::create_can_id(uint32_t priority, uint32_t message_id, uint8_t device_id)
{
  uint32_t can_id{};

  can_id |= (priority & 0x1F) << 24;
  can_id |= (message_id & 0xFFFF) << 8;
  can_id &= ~(1 << 7);
  can_id |= device_id & 0x7F;

  return can_id;
}

void Usb_can_interface::read(uint8_t *p_data, uint32_t len)
{
  frame_recv(m_tty_fd, reinterpret_cast<unsigned char *>(p_data), len);
}

void Usb_can_interface::write(uint32_t can_id, uint8_t *p_data, uint32_t len)
{

  send_data_frame(m_tty_fd, CANUSB_FRAME::CANUSB_FRAME_EXTENDED, static_cast<unsigned long>(can_id),
                  reinterpret_cast<unsigned char *>(p_data), len);
}
