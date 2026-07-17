#pragma once

#include <usb_interface/Usb_can_constants.h>

#include <cstdint>

static bool is_command_frame(const uint8_t *p_frame)
{
  return p_frame[1] == DATA_END_BYTE;
}

struct __attribute__((packed)) Uart_command_package
{
  public:
  void set_canusb_speed(const CANUSB_SPEED _speed)
  {
    speed = _speed;
  }

  void set_mode(const CANUSB_MODE _mode)
  {
    mode = _mode;
  }

  void set_checksum()
  {

    checksum = compute_checksum();
  }

  bool valid_checksum()
  {
    int correct_checksum{compute_checksum()};
    return correct_checksum == checksum;
  }

  private:
  int compute_checksum()
  {
    int _checksum = 0;
    for (int i = 2; i < sizeof(Uart_command_package) - 1; i++)
    {
      _checksum += reinterpret_cast<uint8_t *>(this)[i];
    }
    return _checksum & 0xff;
  }

  const uint8_t sync_byte{SYNC_BYTE};
  const uint8_t command_identifier{COMMAND_IDENTIFIER_BYTE};
  const uint8_t b3{0x12};
  CANUSB_SPEED speed{};
  const CANUSB_FRAME frame_type{CANUSB_FRAME::CANUSB_FRAME_STANDARD};
  const uint8_t b5{0};  /* Filter ID not handled. */
  const uint8_t b6{0};  /* Filter ID not handled. */
  const uint8_t b7{0};  /* Filter ID not handled. */
  const uint8_t b8{0};  /* Filter ID not handled. */
  const uint8_t b9{0};  /* Mask ID not handled. */
  const uint8_t b10{0}; /* Mask ID not handled. */
  const uint8_t b11{0}; /* Mask ID not handled. */
  const uint8_t b12{0}; /* Mask ID not handled. */
  CANUSB_MODE mode{};
  const uint8_t b14{0x01};
  const uint8_t b15{0};
  const uint8_t b16{0};
  const uint8_t b17{0};
  const uint8_t b18{0};
  uint8_t checksum{};
};
