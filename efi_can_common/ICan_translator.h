#include <uavcan/data_type.hpp>
#include <uavcan/driver/can.hpp>
#include <uavcan/node/node.hpp>
#include <uavcan/node/publisher.hpp>
#include <uavcan/node/service_server.hpp>
#include <uavcan/node/subscriber.hpp>
#include <uavcan/transport/perf_counter.hpp>
#include <uavcan/transport/transfer_listener.hpp>

#include <etherform/Demo_broadcast.hpp>
#include <etherform/Echo.hpp>

// #define DO_DEMO_BROADCAST

using namespace uavcan;

template <typename CAN_IF, typename SYS_CLK> class ICan_translator
{
  typedef etherform::Echo echo_t;
  typedef etherform::Demo_broadcast demo_broadcast_t;

  struct Single_can_driver : public ICanDriver
  {
    CAN_IF can_iface;

    Single_can_driver() = default;
    ~Single_can_driver() override = default;

    ICanIface *getIface(uint8_t iface_index) override
    {
      return &can_iface;
    }

    uint8_t getNumIfaces() const override
    {
      return 1;
    }

    int16_t select(CanSelectMasks &inout_masks, const CanFrame *(&pending_tx)[MaxCanIfaces],
                   MonotonicTime blocking_deadline) override
    {
      return 0;
    }
  };

  public:
  ICan_translator(std::string name, uint32_t sw_version, uint32_t hw_version, uint8_t node_id,
                  uint8_t node_priority = 10)
      : m_name(name)
      , m_sw_version(sw_version)
      , m_hw_version(hw_version)
      , m_node_id(node_id)
      , m_node_priority(node_priority)
      , m_echo_service(m_ownship_node)
#ifdef DO_DEMO_BROADCAST
      , m_demo_broadcast(m_ownship_node)
#endif
  {
  }

  virtual ~ICan_translator() = default;

  void main()
  {
    while (1)
    {
      queue_receive();

      do_broadcast();
    }
  }

  bool queue_receive()
  {
    return m_ownship_node.spinOnce();
  }

  void do_broadcast()
  {
#ifdef DO_DEMO_BROADCAST
    demo_broadcast_t msg;
    msg.msg = "Demo from Etherform!";
    int pub_res = m_demo_broadcast.broadcast(msg);

    if (pub_res < 0)
    {
      std::cerr << "Failed to publish demo broadcast." << std::endl;
    }
#endif

    custom_broadcast();
  }

  // This is where you put board specific broadcast functionality
  virtual void custom_broadcast() = 0;

  void initialize()
  {
    // TODO: Stuff for efi echo

    m_ownship_node.setNodeID(uavcan::NodeID(m_node_id));
    m_ownship_node.setName(m_name.c_str());

    uavcan::protocol::SoftwareVersion sw_version;
    sw_version.major = m_sw_version;
    m_ownship_node.setSoftwareVersion(sw_version);

    uavcan::protocol::HardwareVersion hw_version;
    hw_version.major = m_hw_version;
    m_ownship_node.setHardwareVersion(hw_version);

#ifdef DO_DEMO_BROADCAST
    m_demo_broadcast.setTxTimeout(uavcan::MonotonicDuration::fromMSec(1000));
    m_demo_broadcast.setPriority(uavcan::TransferPriority::OneLowerThanHighest);
#endif

    const int srv_start_res = m_echo_service.start([&](const uavcan::ReceivedDataStructure<echo_t::Request> &req,
                                                       echo_t::Response &rsp) { rsp.rsp_message = req.sent_message; });

    if (srv_start_res < 0)
    {
      std::cerr << "Failed to start echo service" << std::endl;
    }

    custom_initialization();

    int node_start_res = m_ownship_node.start({m_node_priority});
    if (node_start_res < 0)
    {
      std::cerr << "Failed to start the node; error: " + node_start_res << std::endl;
    }
    else
    {
      m_ownship_node.setModeOperational();
    }
  }

  virtual void custom_initialization() = 0;

  protected:
  void set_node_priority(uint8_t priority)
  {
    m_node_priority = priority & 0b11111;
  }

  Single_can_driver can_driver{};
  SYS_CLK sys_clock{};

  // TODO: Templatize this size but have default
  uavcan::Node<1024> m_ownship_node{can_driver, sys_clock}; // TODO: Make this private and add protected getter

  private:
  std::string m_name{""};
  uint32_t m_sw_version{0};
  uint32_t m_hw_version{0};
  uint8_t m_node_id{10};
  uint8_t m_node_priority{1 << 1}; // 5 bits or less
  uavcan::ServiceServer<echo_t> m_echo_service;

#ifdef DO_DEMO_BROADCAST
  uavcan::Publisher<demo_broadcast_t> m_demo_broadcast;
#endif
};
