#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <sys/time.h>

#include <asm/termbits.h> /* struct termios2 */

#define CANUSB_INJECT_SLEEP_GAP_DEFAULT 200 /* ms */
#define CANUSB_TTY_BAUD_RATE_DEFAULT 2000000

#define MAX_FRAME_SIZE 15

typedef enum
{
  CANUSB_SPEED_1000000 = 0x01,
  CANUSB_SPEED_800000 = 0x02,
  CANUSB_SPEED_500000 = 0x03,
  CANUSB_SPEED_400000 = 0x04,
  CANUSB_SPEED_250000 = 0x05,
  CANUSB_SPEED_200000 = 0x06,
  CANUSB_SPEED_125000 = 0x07,
  CANUSB_SPEED_100000 = 0x08,
  CANUSB_SPEED_50000 = 0x09,
  CANUSB_SPEED_20000 = 0x0a,
  CANUSB_SPEED_10000 = 0x0b,
  CANUSB_SPEED_5000 = 0x0c,
  CANUSB_SPEED_NOT_SET = 0x0
} CANUSB_SPEED;

typedef enum
{
  CANUSB_MODE_NORMAL = 0x00,
  CANUSB_MODE_LOOPBACK = 0x01,
  CANUSB_MODE_SILENT = 0x02,
  CANUSB_MODE_LOOPBACK_SILENT = 0x03,
} CANUSB_MODE;

typedef enum
{
  CANUSB_FRAME_STANDARD = 0x01,
  CANUSB_FRAME_EXTENDED = 0x02,
} CANUSB_FRAME;

typedef enum
{
  CANUSB_INJECT_PAYLOAD_MODE_RANDOM = 0,
  CANUSB_INJECT_PAYLOAD_MODE_INCREMENTAL = 1,
  CANUSB_INJECT_PAYLOAD_MODE_FIXED = 2,
} CANUSB_PAYLOAD_MODE;

static int terminate_after = 0;
static int program_running = 1;
static int inject_payload_mode = CANUSB_INJECT_PAYLOAD_MODE_FIXED;
static float inject_sleep_gap = CANUSB_INJECT_SLEEP_GAP_DEFAULT;
static int print_traffic = 1;

static CANUSB_SPEED canusb_int_to_speed(int speed)
{
  switch (speed)
  {
  case 1000000:
    return CANUSB_SPEED_1000000;
  case 800000:
    return CANUSB_SPEED_800000;
  case 500000:
    return CANUSB_SPEED_500000;
  case 400000:
    return CANUSB_SPEED_400000;
  case 250000:
    return CANUSB_SPEED_250000;
  case 200000:
    return CANUSB_SPEED_200000;
  case 125000:
    return CANUSB_SPEED_125000;
  case 100000:
    return CANUSB_SPEED_100000;
  case 50000:
    return CANUSB_SPEED_50000;
  case 20000:
    return CANUSB_SPEED_20000;
  case 10000:
    return CANUSB_SPEED_10000;
  case 5000:
    return CANUSB_SPEED_5000;
  default:
    return CANUSB_SPEED_NOT_SET;
  }
}

static int generate_checksum(const unsigned char *data, int data_len)
{
  int i, checksum;

  checksum = 0;
  for (i = 0; i < data_len; i++)
  {
    checksum += data[i];
  }

  return checksum & 0xff;
}

static int frame_is_complete(const unsigned char *frame, int frame_len)
{
  if (frame_len > 0)
  {
    if (frame[0] != 0xaa)
    {
      /* Need to sync on 0xaa at start of frames, so just skip. */
      return 1;
    }
  }

  if (frame_len < 2)
  {
    return 0;
  }

  if (frame[1] == 0x55)
  { /* Command frame... */
    if (frame_len >= 20)
    { /* ...always 20 bytes. */
      return 1;
    }
    else
    {
      return 0;
    }
  }
  else if (((frame[1] >> 4) & 0xc) == 0xc)
  { /* Data frame... */
    if (frame_len >= (frame[1] & 0xf) + 7)
    { /* ...payload and 5 bytes. */
      return 1;
    }
    else
    {
      return 0;
    }
  }

  /* Unhandled frame type. */
  printf("Unhandled frame\n");
  return 0;
}

static int frame_send(int tty_fd, const unsigned char *frame, int frame_len)
{
  int result, i;

  if (print_traffic)
  {
    printf(">>> ");
    for (i = 0; i < frame_len; i++)
    {
      printf("%02x ", frame[i]);
    }
    if (print_traffic > 1)
    {
      printf("    '");
      for (i = 4; i < frame_len - 1; i++)
      {
        printf("%c", isalnum(frame[i]) ? frame[i] : '.');
      }
      printf("'");
    }
    printf("\n");
  }

  result = write(tty_fd, frame, frame_len);
  if (result == -1)
  {
    fprintf(stderr, "write() failed: %s\n", strerror(errno));
    return -1;
  }

  return frame_len;
}

static int frame_recv(int tty_fd, unsigned char *frame, int frame_len_max)
{
  int result, frame_len, checksum;
  unsigned char byte;

  if (print_traffic)
    fprintf(stderr, "<<< ");

  frame_len = 0;
  while (program_running)
  {
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(tty_fd, &readfds);

    timeout.tv_sec = 0;
    timeout.tv_usec = 10000; // 10ms timeout

    int select_result = select(tty_fd + 1, &readfds, NULL, NULL, &timeout);
    if (select_result > 0 && FD_ISSET(tty_fd, &readfds))
    {
      result = read(tty_fd, &byte, 1);
      if (result == -1)
      {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
          fprintf(stderr, "read() failed: %s\n", strerror(errno));
          return -1;
        }
      }
      else if (result > 0)
      {
        if (print_traffic)
          fprintf(stderr, "%02x ", byte);

        if (frame_len == frame_len_max)
        {
          fprintf(stderr, "frame_recv() failed: Overflow\n");
          return -1;
        }

        frame[frame_len++] = byte;

        if (frame_is_complete(frame, frame_len))
        {
          break;
        }
      }
    }
    else if (select_result == 0)
    {
      // Timeout - check if we should shutdown
      if (!program_running)
      {
        break;
      }
      continue;
    }
    else if (select_result == -1)
    {
      // Error occurred
      if (errno != EAGAIN && errno != EWOULDBLOCK)
      {
        fprintf(stderr, "select() failed: %s\n", strerror(errno));
        return -1;
      }
    }

    usleep(10);
  }

  if (print_traffic)
    fprintf(stderr, "\n");

  /* Compare checksum for command frames only. */
  if ((frame_len == 20) && (frame[0] == 0xaa) && (frame[1] == 0x55))
  {
    checksum = generate_checksum(&frame[2], 17);
    if (checksum != frame[frame_len - 1])
    {
      fprintf(stderr, "frame_recv() failed: Checksum incorrect\n");
      return -1;
    }
  }

  return frame_len;
}

static int command_settings(int tty_fd, CANUSB_SPEED speed, CANUSB_MODE mode, CANUSB_FRAME frame)
{
  int cmd_frame_len;
  unsigned char cmd_frame[20];

  cmd_frame_len = 0;
  cmd_frame[cmd_frame_len++] = 0xaa;
  cmd_frame[cmd_frame_len++] = 0x55;
  cmd_frame[cmd_frame_len++] = 0x12;
  cmd_frame[cmd_frame_len++] = speed;
  cmd_frame[cmd_frame_len++] = frame;
  cmd_frame[cmd_frame_len++] = 0; /* Filter ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Filter ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Filter ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Filter ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Mask ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Mask ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Mask ID not handled. */
  cmd_frame[cmd_frame_len++] = 0; /* Mask ID not handled. */
  cmd_frame[cmd_frame_len++] = mode;
  cmd_frame[cmd_frame_len++] = 0x01;
  cmd_frame[cmd_frame_len++] = 0;
  cmd_frame[cmd_frame_len++] = 0;
  cmd_frame[cmd_frame_len++] = 0;
  cmd_frame[cmd_frame_len++] = 0;
  cmd_frame[cmd_frame_len++] = generate_checksum(&cmd_frame[2], 17);

  if (frame_send(tty_fd, cmd_frame, cmd_frame_len) < 0)
  {
    return -1;
  }

  return 0;
}

static int send_data_frame(int tty_fd, CANUSB_FRAME frame, unsigned long id, unsigned char data[], int data_length_code)
{
  int data_frame_len = 0;
  unsigned char data_frame[MAX_FRAME_SIZE] = {0x00};

  if (data_length_code < 0 || data_length_code > 8)
  {
    fprintf(stderr, "Data length code (DLC) must be between 0 and 8!\n");
    return -1;
  }

  /* Byte 0: Packet Start */
  data_frame[data_frame_len++] = 0xaa;

  /* Byte 1: CAN Bus Data Frame Information */
  data_frame[data_frame_len] = 0x00;
  data_frame[data_frame_len] |= 0xC0; /* Bit 7 Always 1, Bit 6 Always 1 */
  if (frame == CANUSB_FRAME_STANDARD)
    data_frame[data_frame_len] &= 0xDF;           /* STD frame */
  else                                            /* CANUSB_FRAME_EXTENDED */
    data_frame[data_frame_len] |= 0x20;           /* EXT frame */
  data_frame[data_frame_len] &= 0xEF;             /* 0=Data */
  data_frame[data_frame_len] |= data_length_code; /* DLC=data_len */
  data_frame_len++;

  /* Byte 2 to 3: ID */
  data_frame[data_frame_len++] = id & 0xFF;        /* lsb */
  data_frame[data_frame_len++] = (id >> 8) & 0xFF; /* msb */
  if (frame == CANUSB_FRAME_EXTENDED)
  {
    /* Byte 4 to 5: Extended ID */
    data_frame[data_frame_len++] = (id >> 16) & 0xFF;
    data_frame[data_frame_len++] = (id >> 24) & 0xFF;
  }

  /* Byte 4 or 6 and on: Data */
  for (int i = 0; i < data_length_code; i++)
    data_frame[data_frame_len++] = data[i];

  /* Last byte: End of frame */
  data_frame[data_frame_len++] = 0x55;

  if (frame_send(tty_fd, data_frame, data_frame_len) < 0)
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