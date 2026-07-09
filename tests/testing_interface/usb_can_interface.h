#pragma once

#include <can_interfaces/can_interface.h>

class Usb_can_interface : public CAN_interface
{
  public:
  explicit Usb_can_interface(const char *tty_device);
  ~Usb_can_interface();

  bool read(uint8_t *p_data, uint32_t len) override;
  void write(uint32_t can_id, uint8_t *p_data, uint32_t len) override;

  private:
  const int m_tty_fd;
};
