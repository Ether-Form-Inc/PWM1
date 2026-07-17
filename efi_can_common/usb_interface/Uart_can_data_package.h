#pragma once

#include <usb_interface/Usb_can_constants.h>

#include <cstdint>

typedef uint16_t standard_header_t;
typedef uint32_t extended_header_t;

static inline uint32_t get_can_frame_size(uint8_t data_info_byte)
{
  return data_info_byte & 0xf;
}

static bool is_data_frame_complete(uint32_t frame_len, uint8_t data_info_byte)
{
  if ((data_info_byte & 0xe0) == 0xc0)
  {
    return frame_len >= get_can_frame_size(data_info_byte) + 5;
    /* This is a standard frame which has payload, 2 id bytes, and 3 protocol bytes. */
  }
  else if ((data_info_byte & 0xe0) == 0xe0)
  {
    /* This is a standard frame which has payload, 4 id bytes, and 3 protocol bytes. */
    return frame_len >= get_can_frame_size(data_info_byte) + 7;
  }

  // Not a data frame
  return false;
}

static bool is_command_frame(const uint8_t *p_frame)
{
  return p_frame[1] == DATA_END_BYTE;
}

static bool is_command_frame_complete(uint32_t frame_len)
{
  /* A command frame is always 20 bytes. */
  return frame_len >= 20;
}

template <typename HEADER> struct __attribute__((packed)) Uart_can_data_package
{
  public:
  void set_data_length(const uint8_t data_size)
  {
    data_frame_info |= data_size;
    compress_data_size(data_size);
  }

  void set_extended_frame()
  {
    data_frame_info |= 0x20;
  }

  void set_header(const uint32_t id)
  {
    memset(&header, 0, sizeof(HEADER));

    header |= (id & 0xFF) << 8;   /* lsb */
    header |= (id & 0xFF00) >> 8; /* msb */

    if (sizeof(HEADER) == 4)
    {
      header |= ((id >> 16) & 0xFF) << 16;
      header |= ((id >> 24) & 0xFF) << 24;
    }
  }

  void set_data(const uint8_t index, const uint8_t data_byte)
  {
    data[index] = data_byte;
  }

  private:
  /************* Data **************/
  const uint8_t begin_sync_byte{SYNC_BYTE};
  uint8_t data_frame_info{0xc0 & 0xef};
  HEADER header;
  uint8_t data[8]; // Constant max size
  const uint8_t end_sync_bytes{DATA_END_BYTE};
  /*********************************/

  // This is used for case where data is not max sized we need to transmit fewer bytes. To this end we move trailing
  // bytes to the end of the data array.
  void compress_data_size(uint8_t data_size)
  {
    if (data_size >= 8)
    {
      return;
    }
    data[data_size] = end_sync_bytes;
  }
};
