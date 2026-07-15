#include <ICan_translator.h>
#include <uavcan/equipment/actuator/ArrayCommand.hpp>
#include <uavcan/equipment/actuator/Command.hpp>
#include <uavcan/equipment/esc/RawCommand.hpp>

using namespace uavcan;

// TODO: This object will be a derived object of  ICAn..
template <typename CAN_IF, typename SYS_CLK> class Pwm_commander : public ICan_translator<CAN_IF, SYS_CLK>
{
  typedef uavcan::equipment::actuator::ArrayCommand servo_handler_type_t;
  typedef uavcan::equipment::esc::RawCommand motor_handler_type_t;

  typedef ICan_translator<CAN_IF, SYS_CLK> Base;

  public:
  Pwm_commander(bool is_servo, uint32_t hw_version, uint8_t node_id, std::string name = "CAN to PWM",
                uint32_t sw_version = 0, uint8_t node_priority = 10)
      : Base(name, sw_version, hw_version, node_id, node_priority)
      , m_servo_broadcast(Base::m_ownship_node)
      , m_motor_broadcast(Base::m_ownship_node)
  {
  }

  ~Pwm_commander() override = default;

  void custom_broadcast() override
  {
  }

  void command_servo_pwm(uint16_t pwm)
  {
    servo_handler_type_t msg;
    uavcan::equipment::actuator::Command cmd;
    cmd.actuator_id = 0;
    cmd.command_type = uavcan::equipment::actuator::Command::COMMAND_TYPE_PWM;
    cmd.command_value = static_cast<float>(pwm);

    msg.commands.push_back(cmd);
    m_servo_broadcast.broadcast(msg);
  }

  void custom_initialization() override
  {
    if (m_is_servo)
    {
      m_servo_broadcast.setTxTimeout(uavcan::MonotonicDuration::fromMSec(1000));
      m_servo_broadcast.setPriority(uavcan::TransferPriority::OneLowerThanHighest);
    }
    else
    {
      m_motor_broadcast.setTxTimeout(uavcan::MonotonicDuration::fromMSec(1000));
      m_motor_broadcast.setPriority(uavcan::TransferPriority::OneLowerThanHighest);
    }
  }

  private:
  bool m_is_servo{true};
  uavcan::Publisher<servo_handler_type_t> m_servo_broadcast;
  uavcan::Publisher<motor_handler_type_t> m_motor_broadcast;
};
