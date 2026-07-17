#pragma once
#include <usb_interface/Uart_can_data_package.h>
#include <usb_interface/Uart_command_package.h>
#include <usb_interface/Usb_can_constants.h>

#include <cstdint>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <asm/termbits.h> /* struct termios2 */

namespace usbcan
{
  static constexpr bool print_traffic{false};

  static bool is_command_frame_complete(uint32_t frame_len)
  {
    /* A command frame is always 20 bytes. */
    return frame_len >= sizeof(Uart_command_package);
  }

  static bool frame_is_complete(uint8_t *frame, int &frame_len)
  {
    // Find the first SYNC_BYTE in the data and set to first byte
    while (frame_len > 1 && frame[0] != SYNC_BYTE)
    {
      memmove(frame, frame + 1, --frame_len);
    }

    // Ensure we have some frame to work with
    if (frame_len <= 0)
    {
      return false;
    }

    /* Need to sync on SYNC_BYTE at start of frames, so just skip. */
    if (frame[0] != SYNC_BYTE)
    {
      frame_len = 0;
      return false;
    }

    if (frame_len < 2)
    {
      return false;
    }

    // Handle frames based on its type
    if (is_command_frame(frame))
    {
      return is_command_frame_complete(frame_len);
    }

    return is_data_frame_complete(frame_len, frame[1]);
  }

  static int frame_send(int tty_fd, const unsigned char *frame, int frame_len)
  {
    if (print_traffic)
    {
      printf(">>> ");
      for (int i = 0; i < frame_len; i++)
      {
        printf("%02x ", frame[i]);
      }
      printf("\n");
    }

    int result = write(tty_fd, frame, frame_len);
    if (result == -1)
    {
      fprintf(stderr, "write() failed: %s\n", strerror(errno));
      return -1;
    }

    return frame_len;
  }

  int do_read(int tty_fd, uint8_t *p_data, size_t size)
  {
    int result = read(tty_fd, p_data, size);

    if (result == -1)
    {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
      {
        fprintf(stderr, "read() failed: %s\n", strerror(errno));
        return -1;
      }
    }

    return result;
  }

  static bool check_command_checksum(uint8_t *p_frame, uint32_t frame_len)
  {
    /* Compare checksum for command frames only. */
    if ((frame_len == 20) && (p_frame[0] == SYNC_BYTE) && (p_frame[1] == 0x55))
    {
      Uart_command_package *p_cmd{reinterpret_cast<Uart_command_package *>(p_frame)};

      bool valid{p_cmd->valid_checksum()};
      if (!valid)
      {
        fprintf(stderr, "frame_recv() failed: Checksum incorrect\n");
        return false;
      }
    }
    return true;
  }

  static int frame_recv(int tty_fd, uint8_t *frame, int frame_len_max)
  {
    int result{};
    int frame_len{};

    while (!frame_is_complete(frame, frame_len))
    {
      // If we have no bytes then recv the minimum for a valid frame. Else rcv 1 at a time lest we rcv multiple frames
      if (frame_len == 0)
      {
        result = do_read(tty_fd, frame + frame_len, MIN_FRAME_SIZE);
      }
      else
      {
        result = do_read(tty_fd, frame + frame_len, 1);
      }

      // Handle data
      if (result > 0)
      {
        frame_len += result;
        if (frame_len > frame_len_max)
        {
          fprintf(stderr, "frame_recv() failed: Overflow\n");
          return -1;
        }
      }
    }

    if (print_traffic)
    {
      fprintf(stderr, "<<< ");
      for (uint32_t i = 0; i < frame_len; i++)
      {
        fprintf(stderr, "%02x ", *(frame + i));
      }
      fprintf(stderr, "\n");
    }

    if (!check_command_checksum(frame, frame_len))
    {
      return -1;
    }

    return frame_len;
  }

  static int command_settings(int tty_fd, CANUSB_SPEED speed, CANUSB_MODE mode)
  {
    Uart_command_package cmd_frame;
    cmd_frame.set_canusb_speed(speed);
    cmd_frame.set_mode(mode);
    cmd_frame.set_checksum();

    if (frame_send(tty_fd, reinterpret_cast<uint8_t *>(&cmd_frame), sizeof(cmd_frame)) < 0)
    {
      return -1;
    }

    return 0;
  }

  // The USB2CAN device converts a UART message to CAN on device.
  template <typename Header_t = extended_header_t>
  static int send_data_frame(int tty_fd, unsigned long id, const uint8_t data[], int data_length_code)
  {
    Uart_can_data_package<Header_t> uart_frame;

    if (sizeof(Header_t) == sizeof(extended_header_t))
    {
      uart_frame.set_extended_frame();
    }

    uart_frame.set_header(id);

    uart_frame.set_data_length(data_length_code);

    for (int i = 0; i < data_length_code; i++)
    {
      uart_frame.set_data(i, data[i]);
    }

    int data_frame_len = 3 + (int)sizeof(Header_t) + (int)data_length_code;

    if (frame_send(tty_fd, reinterpret_cast<uint8_t *>(&uart_frame), data_frame_len) < 0)
    {
      fprintf(stderr, "Unable to send frame!\n");
      return -1;
    }

    return 0;
  }

  static int adapter_init(const char *tty_device, int baudrate)
  {
    int tty_fd, result;

    tty_fd = open(tty_device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (tty_fd == -1)
    {
      fprintf(stderr, "open(%s) failed: %s\n", tty_device, strerror(errno));
      return -1;
    }

    struct termios2 tio;

    result = ioctl(tty_fd, TCGETS2, &tio);
    if (result == -1)
    {
      fprintf(stderr, "ioctl() failed: %s\n", strerror(errno));
      close(tty_fd);
      return -1;
    }

    tio.c_cflag &= ~CBAUD;
    tio.c_cflag = BOTHER | CS8 | CSTOPB;
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_ispeed = baudrate;
    tio.c_ospeed = baudrate;

    result = ioctl(tty_fd, TCSETS2, &tio);
    if (result == -1)
    {
      fprintf(stderr, "ioctl() failed: %s\n", strerror(errno));
      close(tty_fd);
      return -1;
    }

    return tty_fd;
  }

  static int receive_data_frame(const int tty_fd, uint8_t *frame, const int frame_len_max, uint32_t &id)
  {
    uint8_t uart_frame[MAX_FRAME_SIZE];
    int ret_val = frame_recv(tty_fd, uart_frame, frame_len_max + 7);

    if ((uart_frame[1] & 0xe0) == 0xe0)
    {
      // Copy extended id
      memcpy(&id, uart_frame + 2, 4);

      // Copy extended frame back
      memcpy(frame, uart_frame + 6, 8);
      return ret_val - 7;
    }
    else
    {
      // Copy std id mback
      memcpy(&id, uart_frame + 2, 2);

      // Copy standard frame back
      memcpy(frame, uart_frame + 4, 8);
      return ret_val - 5;
    }
  }
};
