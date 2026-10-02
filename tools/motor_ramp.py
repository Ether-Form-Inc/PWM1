#!/usr/bin/env python3
"""Ramps the command 0 -> max -> 0 on the can_to_pwm outputs through the serial USB-CAN adapter, then leaves the
outputs at 0 (the minimum pulse width). REMOVE THE PROPELLERS FIRST.

Motor mode (J5 row 1 open, the default here) sends RawCommand. With --servo (J5 row 1 jumper fitted) it sends
ArrayCommand instead, as UNITLESS (-1 to +1) or PWM (us) commands, so the same 0 -> max -> 0 shape gives the same
pulse widths: 1000 us at 0 and 2000 us at full scale with the default limits.

Usage: motor_ramp.py [-d /dev/ttyUSB0] [--servo [--type unitless|pwm]] [--max 8191] [--outputs 0 1 2 3]
                     [--arm 3] [--up 10] [--hold 2] [--down 10] [--stay]

  --max      peak command (0-8191, 8191 = full scale = the default maximum pulse width)
  --outputs  which of the 4 outputs to drive (the others stay at 0)
  --arm      seconds to hold 0 first, so an ESC can arm
  --up/--hold/--down   seconds spent ramping up, holding the peak, and ramping down
  --stay     after the ramp keep sending 0 until Ctrl-C, instead of sending it for 2 seconds and exiting. Useful to
             tell whether anything at the end of a run comes from the script stopping (or closing the adapter)
Ctrl-C ramps nothing down: it sends 0 for 2 seconds and exits.
"""
import argparse
import sys
import time

import dronecan
import dronecan.transport as transport
import serial

ap = argparse.ArgumentParser()
ap.add_argument("-d", "--device", default="/dev/ttyUSB0")
ap.add_argument("--servo", action="store_true", help="send ArrayCommand (board in servo mode) instead of RawCommand")
ap.add_argument("--type", choices=["unitless", "pwm"], default="unitless", help="servo command type")
ap.add_argument("--max", type=int, default=8191)
ap.add_argument("--outputs", type=int, nargs="+", default=[0, 1, 2, 3])
ap.add_argument("--arm", type=float, default=3.0)
ap.add_argument("--up", type=float, default=10.0)
ap.add_argument("--hold", type=float, default=2.0)
ap.add_argument("--down", type=float, default=10.0)
ap.add_argument("--stay", action="store_true", help="keep sending 0 after the ramp until Ctrl-C")
args = ap.parse_args()
if not 0 <= args.max <= 8191 or any(o not in range(4) for o in args.outputs):
    sys.exit("--max must be 0-8191 and --outputs 0-3")

port = serial.Serial(None, 2000000, timeout=0.01, stopbits=serial.STOPBITS_TWO)
port.port = args.device
port.dtr = True
port.rts = True
port.open()
cfg = bytearray([0xAA, 0x55, 0x12, 0x01, 0x01] + [0] * 8 + [0x00, 0x01, 0, 0, 0, 0])  # 1 Mbit/s
cfg.append(sum(cfg[2:]) & 0xFF)
port.write(cfg)
time.sleep(0.3)

tid = 0


def build(value):
    if not args.servo:
        return dronecan.uavcan.equipment.esc.RawCommand(cmd=[value if i in args.outputs else 0 for i in range(4)])
    frac = value / 8191.0
    cmds = []
    for out in args.outputs:
        if args.type == "unitless":
            cmds.append(dronecan.uavcan.equipment.actuator.Command(actuator_id=out, command_type=0,
                                                                   command_value=-1.0 + 2.0 * frac))
        else:
            cmds.append(dronecan.uavcan.equipment.actuator.Command(actuator_id=out, command_type=4,
                                                                   command_value=1000.0 + 1000.0 * frac))
    return dronecan.uavcan.equipment.actuator.ArrayCommand(commands=cmds)


def send(value):
    global tid
    msg = build(value)
    tr = transport.Transfer(transfer_id=tid & 31, source_node_id=50, payload=msg, transfer_priority=8)
    tid += 1
    for f in tr.to_frames():
        port.write(bytes([0xAA, 0xE0 | len(f.bytes)]) + f.message_id.to_bytes(4, "little") + bytes(f.bytes) + b"\x55")
        time.sleep(0.003)


def run(seconds, start, end, label):
    t0 = time.time()
    last_print = -1
    while True:
        frac = (time.time() - t0) / seconds if seconds > 0 else 1.0
        frac = min(frac, 1.0)
        value = int(round(start + (end - start) * frac))
        send(value)
        if int(time.time() - t0) != last_print:
            last_print = int(time.time() - t0)
            print(f"{label:5s} value {value:5d}  ({100.0 * value / 8191:5.1f} %)", flush=True)
        if frac >= 1.0:
            return
        time.sleep(0.05)   # 20 Hz, well inside the 5 s motor timeout


try:
    run(args.arm, 0, 0, "arm")
    run(args.up, 0, args.max, "up")
    run(args.hold, args.max, args.max, "hold")
    run(args.down, args.max, 0, "down")
    if args.stay:
        print("done: staying at 0, Ctrl-C to stop", flush=True)
        while True:
            send(0)
            time.sleep(0.05)
    print("done: holding 0 for 2 s")
except KeyboardInterrupt:
    print("\nstopped: sending 0")
finally:
    t0 = time.time()
    while time.time() - t0 < 2.0:
        send(0)
        time.sleep(0.05)
