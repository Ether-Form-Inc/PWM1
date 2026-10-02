// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ether Form Inc.
#pragma once

// DroneCAN PWM node built on libcanard. In servo mode it subscribes to ArrayCommand, in motor mode to RawCommand, and
// it drives the four PCA9685 outputs. It publishes NodeStatus at 1 Hz and answers GetNodeInfo, GetSet and
// ExecuteOpcode (the node ID, pulse width limits and motor timeout parameters), RestartNode and etherform.Echo.
// All state is static; nothing is allocated outside the libcanard pool.

#include <Pwm_config.h>
#include <Stm_fdcan.h>
#include <Stm_pca9685.h>
#include <Stm_settings.h>

extern "C"
{
#include <canard.h>
#include <etherform.Echo.h>
#include <uavcan.equipment.actuator.ArrayCommand.h>
#include <uavcan.equipment.esc.RawCommand.h>
#include <uavcan.protocol.GetNodeInfo.h>
#include <uavcan.protocol.NodeStatus.h>
#include <uavcan.protocol.RestartNode.h>
#include <uavcan.protocol.param.ExecuteOpcode.h>
#include <uavcan.protocol.param.GetSet.h>
}

#include <cstring>

class Canard_pwm_node
{
public:
  // actuator_id is the id of output 0 (the J5 offset); outputs 1 to 3 are the next ids
  Canard_pwm_node(uint8_t node_id, bool is_servo, uint32_t actuator_id)
      : node_id(node_id)
      , is_servo(is_servo)
      , actuator_id(actuator_id)
  {
  }

  // Never returns
  void run()
  {
    // Pulse width limits and timeout: the defaults, overridden by any values stored in flash
    uint16_t stored[Pwm::setting_count];
    Stm_settings::read_pwm_ranges(stored);
    config.load(stored);

    fdcan.init();
    canardInit(&canard, pool, sizeof(pool), on_transfer_received, should_accept_transfer, this);
    canardSetLocalNodeID(&canard, node_id);
    pca.init(); // a chip that does not answer is retried by service_pca()

    for (;;)
    {
      service_rx(HAL_GetTick());

      // Read the time after receiving: handlers for commands received just now record a later time than one taken
      // before service_rx(), which would make "now minus last" wrap around to a huge value
      const uint32_t now_ms = HAL_GetTick();
      service_bus_state(now_ms);
      service_restart(now_ms);
      service_pca(now_ms);
      service_motor_timeout(now_ms);
      service_node_status(now_ms);
      service_tx();
    }
  }

  // Vendor specific status codes in NodeStatus
  static constexpr uint16_t status_ok = 0;
  static constexpr uint16_t status_pca9685_not_answering = 1;

private:
  static constexpr uint32_t node_status_period_ms = 1000;
  static constexpr uint32_t cleanup_period_ms = 1000;
  static constexpr uint32_t bus_off_retry_ms = 100;
  static constexpr uint32_t restart_timeout_ms = 500;
  static constexpr uint8_t sw_version_major = 0;
  static constexpr uint8_t sw_version_minor = 2;
  static constexpr int64_t node_id_param_max = 127;
  // NodeStatus priority: motor mode uses the highest-but-one priority class the firmware has always used, servo 20
  static constexpr uint8_t priority_motor = 1;
  static constexpr uint8_t priority_servo = 20;

  // ---- receive ----
  void service_rx(uint32_t now_ms)
  {
    uint32_t id;
    uint8_t len;
    CanardCANFrame frame;
    // Bounded so a busy bus cannot starve everything else
    for (int i = 0; i < 16; i++)
    {
      memset(&frame, 0, sizeof(frame));
      if (!fdcan.receive(id, frame.data, len))
      {
        break;
      }
      frame.id = id | CANARD_CAN_FRAME_EFF;
      frame.data_len = len;
      canardHandleRxFrame(&canard, &frame, (uint64_t)now_ms * 1000ULL);
    }

    if ((uint32_t)(now_ms - last_cleanup_ms) >= cleanup_period_ms)
    {
      last_cleanup_ms = now_ms;
      canardCleanupStaleTransfers(&canard, (uint64_t)now_ms * 1000ULL);
    }
  }

  static bool should_accept_transfer(const CanardInstance *ins, uint64_t *out_signature, uint16_t data_type_id,
                                     CanardTransferType transfer_type, uint8_t)
  {
    const Canard_pwm_node *self = static_cast<const Canard_pwm_node *>(canardGetUserReference(ins));
    if (transfer_type == CanardTransferTypeBroadcast)
    {
      if (self->is_servo && data_type_id == UAVCAN_EQUIPMENT_ACTUATOR_ARRAYCOMMAND_ID)
      {
        *out_signature = UAVCAN_EQUIPMENT_ACTUATOR_ARRAYCOMMAND_SIGNATURE;
        return true;
      }
      if (!self->is_servo && data_type_id == UAVCAN_EQUIPMENT_ESC_RAWCOMMAND_ID)
      {
        *out_signature = UAVCAN_EQUIPMENT_ESC_RAWCOMMAND_SIGNATURE;
        return true;
      }
      return false;
    }
    if (transfer_type != CanardTransferTypeRequest)
    {
      return false;
    }
    switch (data_type_id)
    {
    case UAVCAN_PROTOCOL_GETNODEINFO_ID:
      *out_signature = UAVCAN_PROTOCOL_GETNODEINFO_SIGNATURE;
      return true;
    case UAVCAN_PROTOCOL_PARAM_GETSET_ID:
      *out_signature = UAVCAN_PROTOCOL_PARAM_GETSET_SIGNATURE;
      return true;
    case UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_ID:
      *out_signature = UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_SIGNATURE;
      return true;
    case UAVCAN_PROTOCOL_RESTARTNODE_ID:
      *out_signature = UAVCAN_PROTOCOL_RESTARTNODE_SIGNATURE;
      return true;
    case ETHERFORM_ECHO_REQUEST_ID:
      *out_signature = ETHERFORM_ECHO_REQUEST_SIGNATURE;
      return true;
    default:
      return false;
    }
  }

  static void on_transfer_received(CanardInstance *ins, CanardRxTransfer *transfer)
  {
    Canard_pwm_node *self = static_cast<Canard_pwm_node *>(canardGetUserReference(ins));
    if (transfer->transfer_type == CanardTransferTypeBroadcast)
    {
      if (transfer->data_type_id == UAVCAN_EQUIPMENT_ACTUATOR_ARRAYCOMMAND_ID)
      {
        self->handle_servo_set(transfer);
      }
      else if (transfer->data_type_id == UAVCAN_EQUIPMENT_ESC_RAWCOMMAND_ID)
      {
        self->handle_motor_set(transfer);
      }
      return;
    }
    if (transfer->transfer_type != CanardTransferTypeRequest)
    {
      return;
    }
    switch (transfer->data_type_id)
    {
    case UAVCAN_PROTOCOL_GETNODEINFO_ID:
      self->send_node_info(transfer);
      break;
    case UAVCAN_PROTOCOL_PARAM_GETSET_ID:
      self->handle_param_getset(transfer);
      break;
    case UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_ID:
      self->handle_param_execute_opcode(transfer);
      break;
    case UAVCAN_PROTOCOL_RESTARTNODE_ID:
      self->handle_restart_node(transfer);
      break;
    case ETHERFORM_ECHO_REQUEST_ID:
      self->handle_echo(transfer);
      break;
    default:
      break;
    }
  }

  // ---- commands ----

  // The id of output 0 is the J5 offset; ids offset to offset + 3 are the outputs of this board
  void handle_servo_set(CanardRxTransfer *transfer)
  {
    static uavcan_equipment_actuator_ArrayCommand msg;
    memset(&msg, 0, sizeof(msg));
    const bool decode_failed = uavcan_equipment_actuator_ArrayCommand_decode(transfer, &msg);
    if (decode_failed)
    {
      return;
    }
    pwm_debug_stats.servo_messages = pwm_debug_stats.servo_messages + 1;
    for (uint32_t c = 0; c < msg.commands.len; c++)
    {
      const uavcan_equipment_actuator_Command &cmd = msg.commands.data[c];
      if (cmd.actuator_id < actuator_id)
      {
        continue;
      }
      const uint32_t output = cmd.actuator_id - actuator_id;
      if (output >= Pwm::num_outputs)
      {
        continue;
      }
      if (cmd.command_type == UAVCAN_EQUIPMENT_ACTUATOR_COMMAND_COMMAND_TYPE_PWM)
      {
        pca.set_count(output, Pwm::pulse_to_count(config.servo_pwm_us(output, cmd.command_value)));
      }
      else if (cmd.command_type == UAVCAN_EQUIPMENT_ACTUATOR_COMMAND_COMMAND_TYPE_UNITLESS)
      {
        pca.set_count(output, Pwm::pulse_to_count(config.servo_unitless_us(output, cmd.command_value)));
      }
    }
  }

  // The command array is indexed by actuator id: entries offset to offset + 3 drive outputs 0 to 3
  void handle_motor_set(CanardRxTransfer *transfer)
  {
    static uavcan_equipment_esc_RawCommand msg;
    memset(&msg, 0, sizeof(msg));
    const bool decode_failed = uavcan_equipment_esc_RawCommand_decode(transfer, &msg);
    if (decode_failed)
    {
      return;
    }
    pwm_debug_stats.motor_messages = pwm_debug_stats.motor_messages + 1;
    // Any RawCommand message restarts the motor timeout
    last_motor_msg_ms = HAL_GetTick();
    motor_outputs_active = true;

    // Stop at the smaller of this board's last id and the size of the message
    const uint32_t loop_until = msg.cmd.len > actuator_id + Pwm::num_outputs ? actuator_id + Pwm::num_outputs : msg.cmd.len;
    for (uint32_t c = actuator_id; c < loop_until; c++)
    {
      const uint32_t output = c - actuator_id;
      pca.set_count(output, Pwm::pulse_to_count(config.motor_us(output, msg.cmd.data[c])));
    }
  }

  // Motor mode only: switch the PWM signals off if no RawCommand has arrived for the motor timeout. They come back
  // with the next RawCommand. Servo outputs hold their last value.
  void service_motor_timeout(uint32_t now_ms)
  {
    if (!is_servo && motor_outputs_active && (uint32_t)(now_ms - last_motor_msg_ms) >= config.timeout_ms())
    {
      for (uint8_t c = 0; c < Pwm::num_outputs; c++)
      {
        pca.set_off(c);
      }
      motor_outputs_active = false;
      pwm_debug_stats.motor_timeouts = pwm_debug_stats.motor_timeouts + 1;
    }
  }

  void service_pca(uint32_t now_ms)
  {
    pca.service(now_ms);
  }

  // ---- node information ----
  void fill_node_status(uavcan_protocol_NodeStatus &status)
  {
    status.uptime_sec = HAL_GetTick() / 1000;
    status.mode = UAVCAN_PROTOCOL_NODESTATUS_MODE_OPERATIONAL;
    status.sub_mode = 0;
    if (pca.ok())
    {
      status.health = UAVCAN_PROTOCOL_NODESTATUS_HEALTH_OK;
      status.vendor_specific_status_code = status_ok;
    }
    else
    {
      status.health = UAVCAN_PROTOCOL_NODESTATUS_HEALTH_ERROR;
      status.vendor_specific_status_code = status_pca9685_not_answering;
    }
  }

  void send_response(CanardRxTransfer *request, uint64_t signature, uint16_t type_id, const uint8_t *buf, uint32_t len)
  {
    canardRequestOrRespond(&canard, request->source_node_id, signature, type_id, &request->transfer_id,
                           request->priority, CanardResponse, buf, (uint16_t)len);
  }

  void send_node_info(CanardRxTransfer *request)
  {
    static uavcan_protocol_GetNodeInfoResponse res;
    static uint8_t buf[UAVCAN_PROTOCOL_GETNODEINFO_RESPONSE_MAX_SIZE];
    memset(&res, 0, sizeof(res));
    memset(buf, 0, sizeof(buf));

    fill_node_status(res.status);
    res.software_version.major = sw_version_major;
    res.software_version.minor = sw_version_minor;
    res.hardware_version.major = 0;

    // 96 bit STM32 unique ID, zero padded to the 128 bits DroneCAN wants
    const uint32_t *uid = reinterpret_cast<const uint32_t *>(0x1FFF7590UL);
    memcpy(res.hardware_version.unique_id, uid, 12);

    static const char name[] = "CAN to PWM";
    res.name.len = sizeof(name) - 1;
    memcpy(res.name.data, name, res.name.len);

    const uint32_t len = uavcan_protocol_GetNodeInfoResponse_encode(&res, buf
#if CANARD_ENABLE_TAO_OPTION
                                                                   , false
#endif
    );
    send_response(request, UAVCAN_PROTOCOL_GETNODEINFO_SIGNATURE, UAVCAN_PROTOCOL_GETNODEINFO_ID, buf, len);
  }

  void service_node_status(uint32_t now_ms)
  {
    if ((uint32_t)(now_ms - last_node_status_ms) < node_status_period_ms)
    {
      return;
    }
    last_node_status_ms = now_ms;

    uavcan_protocol_NodeStatus status;
    uint8_t buf[UAVCAN_PROTOCOL_NODESTATUS_MAX_SIZE];
    memset(buf, 0, sizeof(buf));
    fill_node_status(status);
    const uint32_t len = uavcan_protocol_NodeStatus_encode(&status, buf
#if CANARD_ENABLE_TAO_OPTION
                                                          , false
#endif
    );
    canardBroadcast(&canard, UAVCAN_PROTOCOL_NODESTATUS_SIGNATURE, UAVCAN_PROTOCOL_NODESTATUS_ID,
                    &node_status_transfer_id, is_servo ? priority_servo : priority_motor, buf, (uint16_t)len);
  }

  // etherform.Echo, a link test: the response carries the request's bytes back
  void handle_echo(CanardRxTransfer *request)
  {
    etherform_EchoRequest req;
    memset(&req, 0, sizeof(req));
    const bool decode_failed = etherform_EchoRequest_decode(request, &req);
    canardReleaseRxTransferPayload(&canard, request);
    if (decode_failed)
    {
      return;
    }
    etherform_EchoResponse res;
    memset(&res, 0, sizeof(res));
    res.rsp_message.len = req.sent_message.len;
    memcpy(res.rsp_message.data, req.sent_message.data, req.sent_message.len);
    uint8_t buf[ETHERFORM_ECHO_RESPONSE_MAX_SIZE];
    memset(buf, 0, sizeof(buf));
    const uint32_t len = etherform_EchoResponse_encode(&res, buf
#if CANARD_ENABLE_TAO_OPTION
                                                       , false
#endif
    );
    send_response(request, ETHERFORM_ECHO_RESPONSE_SIGNATURE, ETHERFORM_ECHO_RESPONSE_ID, buf, len);
  }

  // ---- parameters ----
  //
  // Parameter 0 is NODE_ID (0 means "not set": the default applies), stored in flash and used from the next start.
  // Parameters 1 to 17 are the pulse width limits (us) and the motor timeout (ms) of Pwm::Config, applied at once.
  // Writing 0 to a parameter restores its default.
  static int64_t stored_node_id()
  {
    uint8_t v;
    return Stm_settings::read(Stm_settings::node_id, v) ? v : 0;
  }

  void assign_param(uint32_t index, int64_t wanted)
  {
    if (index == 0)
    {
      // An out of range value is ignored; the response shows what is actually stored
      if (wanted >= 0 && wanted <= node_id_param_max && wanted != stored_node_id())
      {
        if (wanted == 0)
        {
          Stm_settings::erase(Stm_settings::node_id);
        }
        else
        {
          Stm_settings::write(Stm_settings::node_id, (uint8_t)wanted);
        }
      }
      return;
    }
    uint16_t candidate[Pwm::setting_count];
    if (config.propose(index - 1, wanted, candidate) && Stm_settings::write_pwm_ranges(candidate))
    {
      config.commit(candidate);
    }
  }

  void handle_param_getset(CanardRxTransfer *request)
  {
    static uavcan_protocol_param_GetSetRequest req;
    static uavcan_protocol_param_GetSetResponse res;
    static uint8_t buf[UAVCAN_PROTOCOL_PARAM_GETSET_RESPONSE_MAX_SIZE];

    memset(&req, 0, sizeof(req));
    const bool decode_failed = uavcan_protocol_param_GetSetRequest_decode(request, &req);
    canardReleaseRxTransferPayload(&canard, request); // give the pool blocks back before replying
    if (decode_failed)
    {
      return;
    }

    // Find the parameter by name if one is given, otherwise by index
    int index;
    if (req.name.len > 0)
    {
      index = Pwm::find_param(req.name.data, req.name.len);
    }
    else
    {
      index = req.index < Pwm::param_count ? (int)req.index : -1;
    }

    memset(&res, 0, sizeof(res));
    if (index >= 0)
    {
      if (req.value.union_tag == UAVCAN_PROTOCOL_PARAM_VALUE_INTEGER_VALUE)
      {
        assign_param((uint32_t)index, req.value.integer_value);
      }
      const bool is_node_id = index == 0;
      const uint32_t setting = (uint32_t)index - 1;
      res.value.union_tag = UAVCAN_PROTOCOL_PARAM_VALUE_INTEGER_VALUE;
      res.value.integer_value = is_node_id ? stored_node_id() : (int64_t)config.effective(setting);
      res.default_value.union_tag = UAVCAN_PROTOCOL_PARAM_VALUE_INTEGER_VALUE;
      res.default_value.integer_value = is_node_id ? 0 : (int64_t)Pwm::default_setting(setting);
      res.max_value.union_tag = UAVCAN_PROTOCOL_PARAM_NUMERICVALUE_INTEGER_VALUE;
      res.max_value.integer_value = is_node_id ? node_id_param_max
                                    : (setting == Pwm::timeout_index ? Pwm::timeout_param_max : Pwm::limit_param_max);
      res.min_value.union_tag = UAVCAN_PROTOCOL_PARAM_NUMERICVALUE_INTEGER_VALUE;
      res.min_value.integer_value = 0;
      char name[Pwm::param_name_max];
      Pwm::param_name((uint32_t)index, name);
      res.name.len = (uint8_t)strlen(name);
      memcpy(res.name.data, name, res.name.len);
    }
    // A parameter that does not exist gets an empty response, which ends a listing by index

    memset(buf, 0, sizeof(buf));
    const uint32_t len = uavcan_protocol_param_GetSetResponse_encode(&res, buf
#if CANARD_ENABLE_TAO_OPTION
                                                                     , false
#endif
    );
    send_response(request, UAVCAN_PROTOCOL_PARAM_GETSET_SIGNATURE, UAVCAN_PROTOCOL_PARAM_GETSET_ID, buf, len);
  }

  void handle_param_execute_opcode(CanardRxTransfer *request)
  {
    uavcan_protocol_param_ExecuteOpcodeRequest req = {};
    const bool decode_failed = uavcan_protocol_param_ExecuteOpcodeRequest_decode(request, &req);
    canardReleaseRxTransferPayload(&canard, request);
    if (decode_failed)
    {
      return;
    }

    uavcan_protocol_param_ExecuteOpcodeResponse res = {};
    if (req.opcode == UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_REQUEST_OPCODE_SAVE)
    {
      res.ok = true; // GetSet already stored the values in flash
    }
    else if (req.opcode == UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_REQUEST_OPCODE_ERASE)
    {
      // Back to the default node ID, pulse width limits and motor timeout
      res.ok = Stm_settings::erase(Stm_settings::node_id) && Stm_settings::erase(Stm_settings::pwm_ranges);
      config.clear();
    }

    uint8_t buf[UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_RESPONSE_MAX_SIZE] = {};
    const uint32_t len = uavcan_protocol_param_ExecuteOpcodeResponse_encode(&res, buf
#if CANARD_ENABLE_TAO_OPTION
                                                                            , false
#endif
    );
    send_response(request, UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_SIGNATURE, UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_ID, buf,
                  len);
  }

  void handle_restart_node(CanardRxTransfer *request)
  {
    uavcan_protocol_RestartNodeRequest req = {};
    const bool decode_failed = uavcan_protocol_RestartNodeRequest_decode(request, &req);
    canardReleaseRxTransferPayload(&canard, request);
    if (decode_failed)
    {
      return;
    }

    uavcan_protocol_RestartNodeResponse res = {};
    res.ok = req.magic_number == UAVCAN_PROTOCOL_RESTARTNODE_REQUEST_MAGIC_NUMBER;
    if (res.ok)
    {
      restart_requested = true; // service_restart() resets once the reply has been sent
      restart_requested_ms = HAL_GetTick();
    }

    uint8_t buf[UAVCAN_PROTOCOL_RESTARTNODE_RESPONSE_MAX_SIZE] = {};
    const uint32_t len = uavcan_protocol_RestartNodeResponse_encode(&res, buf
#if CANARD_ENABLE_TAO_OPTION
                                                                    , false
#endif
    );
    send_response(request, UAVCAN_PROTOCOL_RESTARTNODE_SIGNATURE, UAVCAN_PROTOCOL_RESTARTNODE_ID, buf, len);
  }

  // Resets the MCU after a RestartNode request, once the reply has left the CAN controller
  void service_restart(uint32_t now_ms)
  {
    if (!restart_requested)
    {
      return;
    }
    const bool sent = canardPeekTxQueue(&canard) == nullptr && fdcan.tx_idle();
    if (sent || (uint32_t)(now_ms - restart_requested_ms) > restart_timeout_ms)
    {
      HAL_Delay(5); // let the last frame finish on the bus
      NVIC_SystemReset();
    }
  }

  // ---- transmit ----

  // Recovers from bus-off. Frames queued while the controller cannot transmit are dropped.
  void service_bus_state(uint32_t now_ms)
  {
    if (!fdcan.bus_off())
    {
      bus_off_seen = false;
      return;
    }

    while (canardPeekTxQueue(&canard) != nullptr)
    {
      canardPopTxQueue(&canard);
    }

    if (!bus_off_seen)
    {
      bus_off_seen = true;
      last_bus_off_action_ms = now_ms;
    }
    else if ((uint32_t)(now_ms - last_bus_off_action_ms) >= bus_off_retry_ms)
    {
      fdcan.restart_after_bus_off();
      last_bus_off_action_ms = now_ms;
    }
  }

  void service_tx()
  {
#ifdef CAN_TX_MIN_SPACING_MS
    // Test aid for slow USB-CAN adapters whose serial link drops back-to-back frames
    static uint32_t last_tx_ms = 0;
    if ((uint32_t)(HAL_GetTick() - last_tx_ms) < CAN_TX_MIN_SPACING_MS)
    {
      return;
    }
#endif

    // The hardware FIFO only holds 3 frames, so feed it as it drains
    while (fdcan.tx_has_room())
    {
      const CanardCANFrame *frame = canardPeekTxQueue(&canard);
      if (frame == nullptr)
      {
        break;
      }
      if (!fdcan.send(frame->id, frame->data, frame->data_len))
      {
        break;
      }
      canardPopTxQueue(&canard);
#ifdef CAN_TX_MIN_SPACING_MS
      last_tx_ms = HAL_GetTick();
      break;
#endif
    }
  }

  uint8_t node_id;
  const bool is_servo;
  const uint32_t actuator_id;
  Stm_fdcan fdcan;
  Stm_pca9685 pca;
  Pwm::Config config;

  CanardInstance canard;
  alignas(8) uint8_t pool[2048];

  uint8_t node_status_transfer_id{0};
  uint32_t last_node_status_ms{0};
  uint32_t last_cleanup_ms{0};
  uint32_t last_bus_off_action_ms{0};
  bool bus_off_seen{false};
  bool restart_requested{false};
  uint32_t restart_requested_ms{0};
  uint32_t last_motor_msg_ms{0};
  bool motor_outputs_active{false};
};
