#include <atomic>
#include <cstring>
#include <iostream>
#include <signal.h>

#include <testing_interface/usb_can_interface.h>
#include <testing_interface/usbcan.h>

class Loopback
{
  public:
  // void start()
  //{
  //   std::cout << "Loopback running press ctrl-c to shutdown." << std::endl;

  //  Usb_can_interface interface{"/dev/ttyUSB2"};
  //  uint8_t frame[MAX_FRAME_SIZE]; // Max frame size?

  //  while (m_running)
  //  {
  //    // if (interface.read(frame, sizeof(frame)))
  //    {
  //      // uint32_t message_id{Drone_can::get_message_id(frame)};
  //      // std::cout << "Received message id: " << message_id << std::endl;
  //      std::cout << "mid not impl" << std::endl;
  //    }
  //  }
  //}

  void stop()
  {
    m_running = false;
  }

  private:
  std::atomic<bool> m_running{true};
};

static inline Loopback l;

static void signal_receiver(int sig)
{
  l.stop();
}

static void register_signal_receiver()
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_receiver;
  sigaction(SIGINT, &sa, NULL);
}

int main()
{
  // Initialize
  register_signal_receiver();
  // l.start();
  return 0;
}
