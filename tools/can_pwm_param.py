#!/usr/bin/env python3
"""Reads and sets the can_to_pwm parameters over CAN through the serial USB-CAN adapter, using the reference
DroneCAN library (pip install dronecan pyserial) to build the requests and decode the replies.

Usage: can_pwm_param.py [-d /dev/ttyUSB0] NODE_ID command [name] [value]

  list                 show every parameter
  get NAME             show one parameter
  set NAME VALUE       store a value (applied at once for the PWM limits, after a restart for NODE_ID)
  set-range KIND OUT MIN MAX
                       set both pulse width limits (us) of one output, KIND is servo or motor, OUT is 0 to 3.
                       The limits are written in an order that never leaves max <= min.
  erase                clear every stored value, back to the defaults
  restart              restart the node (applies a new NODE_ID)

Parameters: NODE_ID (1-127, 0 = default 10), and the pulse width limits in us (1-20000, 0 = default)
SERVO_MIN_US_0..3, SERVO_MAX_US_0..3, MOTOR_MIN_US_0..3, MOTOR_MAX_US_0..3.
"""
import argparse
import sys
import time

import dronecan
import dronecan.transport as transport
import serial

SELF = 50  # our own node ID on the bus

ap = argparse.ArgumentParser(add_help=False)
ap.add_argument("-d", "--device", default="/dev/ttyUSB0")
ap.add_argument("node", type=int)
ap.add_argument("command", choices=["list", "get", "set", "set-range", "erase", "restart"])
ap.add_argument("args", nargs="*")
args = ap.parse_args()

port = serial.Serial(None, 2000000, timeout=0.02, stopbits=serial.STOPBITS_TWO)
port.port = args.device
port.dtr = True   # the adapter only streams while DTR and RTS are asserted
port.rts = True
port.open()
cfg = bytearray([0xAA, 0x55, 0x12, 0x01, 0x01] + [0] * 8 + [0x00, 0x01, 0, 0, 0, 0])  # 1 Mbit/s, normal
cfg.append(sum(cfg[2:]) & 0xFF)
port.write(cfg)
time.sleep(0.3)
port.reset_input_buffer()

manager = transport.TransferManager()
buf = bytearray()
transfer_id = int(time.time() * 7) & 31


def send(payload):
    """Sends a service request and returns the reply payload, or None on timeout."""
    global transfer_id, buf
    tr = transport.Transfer(transfer_id=transfer_id, source_node_id=SELF, dest_node_id=args.node,
                            payload=payload, transfer_priority=20,
                            request_not_response=True, service_not_message=True)
    transfer_id = (transfer_id + 1 + int(time.time() * 1000) % 7) & 31
    for frame in tr.to_frames():
        port.write(bytes([0xAA, 0xE0 | len(frame.bytes)]) + frame.message_id.to_bytes(4, "little") + bytes(frame.bytes) + b"\x55")
        time.sleep(0.003)   # the adapter drops frames sent back to back
    deadline = time.time() + 0.5
    while time.time() < deadline:
        buf += port.read(4096)
        i = 0
        while i < len(buf):
            if buf[i] == 0xAA and i + 1 < len(buf) and (buf[i + 1] & 0xC0) == 0xC0:
                end = i + 6 + (buf[i + 1] & 15)
                if end >= len(buf):
                    break
                if buf[end] == 0x55:
                    frame = transport.Frame(int.from_bytes(buf[i + 2:i + 6], "little"), bytes(buf[i + 6:end]))
                    frames = manager.receive_frame(frame)
                    if frames:
                        try:
                            done = transport.Transfer()
                            done.from_frames(frames)
                        except transport.TransferError:
                            done = None
                        if done is not None and done.source_node_id == args.node and done.dest_node_id == SELF \
                                and not done.request_not_response:
                            del buf[:end + 1]
                            return done.payload
                    i = end + 1
                    continue
            i += 1
        del buf[:i]
    return None


def request(payload, tries=12):
    """The USB-CAN adapter drops some frames, so repeat a request (gets and sets are harmless to repeat)."""
    for _ in range(tries):
        res = send(payload)
        if res is not None:
            return res
    sys.exit("no reply")


def getset(index=0, name="", value=None):
    req = dronecan.uavcan.protocol.param.GetSet.Request(index=index, name=name)
    if value is not None:
        req.value = dronecan.uavcan.protocol.param.Value(integer_value=value)
    return request(req)


def show(res):
    name = res.name.decode() if isinstance(res.name, (bytes, bytearray)) else str(res.name)
    if not name:
        return None
    print(f"{name:16s} = {res.value.integer_value:6d}   default {res.default_value.integer_value:5d}"
          f"   min {res.min_value.integer_value}   max {res.max_value.integer_value}")
    return res.value.integer_value


def set_param(name, value):
    res = getset(name=name, value=value)
    got = show(res)
    if got is None:
        sys.exit(f"{name}: no such parameter")
    shown = got
    if value != 0 and shown != value:
        print(f"  not applied: the board kept {shown} (value out of range, or max would not be above min)")
    return shown


cmd, a = args.command, args.args
if cmd == "list":
    index = 0
    while show(getset(index=index)) is not None:
        index += 1
elif cmd == "get":
    if len(a) != 1:
        sys.exit("get needs a parameter name")
    if show(getset(name=a[0])) is None:
        sys.exit("no such parameter")
elif cmd == "set":
    if len(a) != 2:
        sys.exit("set needs a name and a value")
    set_param(a[0], int(a[1]))
elif cmd == "set-range":
    if len(a) != 4 or a[0] not in ("servo", "motor") or not a[1] in "0123":
        sys.exit("set-range needs: servo|motor OUTPUT(0-3) MIN MAX")
    kind, out, lo, hi = a[0].upper(), a[1], int(a[2]), int(a[3])
    if not 0 < lo < hi <= 20000:
        sys.exit("need 0 < MIN < MAX <= 20000")
    cur_lo = getset(name=f"{kind}_MIN_US_{out}").value.integer_value
    cur_hi = getset(name=f"{kind}_MAX_US_{out}").value.integer_value
    # Raise the maximum first when moving up, lower the minimum first when moving down, so max > min throughout
    order = [("MAX", hi), ("MIN", lo)] if lo >= cur_lo else [("MIN", lo), ("MAX", hi)]
    for which, v in order:
        set_param(f"{kind}_{which}_US_{out}", v)
elif cmd == "erase":
    res = request(dronecan.uavcan.protocol.param.ExecuteOpcode.Request(opcode=1))
    print("erase:", "ok" if res.ok else "failed")
elif cmd == "restart":
    res = request(dronecan.uavcan.protocol.RestartNode.Request(magic_number=dronecan.uavcan.protocol.RestartNode.Request().MAGIC_NUMBER))
    print("restart:", "accepted" if res.ok else "refused")
