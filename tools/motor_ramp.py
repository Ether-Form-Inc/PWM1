#!/usr/bin/env python3
"""Ramps the RawCommand motor value 0 -> max -> 0 on the can_to_pwm outputs through the serial USB-CAN adapter, then
leaves the outputs at 0. Needs the board in motor mode (J5 row 1 open). REMOVE THE PROPELLERS FIRST.

Usage: motor_ramp.py [-d /dev/ttyUSB0] [--max 8191] [--outputs 0 1 2 3] [--arm 3] [--up 10] [--hold 2] [--down 10]

  --max      peak RawCommand value (0-8191, 8191 = full scale = the default maximum pulse width)
  --outputs  which of the 4 outputs to drive (the others stay at 0)
  --arm      seconds to hold 0 first, so an ESC can arm
  --up/--hold/--down   seconds spent ramping up, holding the peak, and ramping down
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
ap.add_argument("--max", type=int, default=8191)
ap.add_argument("--outputs", type=int, nargs="+", default=[0, 1, 2, 3])
ap.add_argument("--arm", type=float, default=3.0)
ap.add_argument("--up", type=float, default=10.0)
ap.add_argument("--hold", type=float, default=2.0)
ap.add_argument("--down", type=float, default=10.0)
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


def send(value):
    global tid
    cmd = [value if i in args.outputs else 0 for i in range(4)]
    msg = dronecan.uavcan.equipment.esc.RawCommand(cmd=cmd)
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
    print("done: holding 0 for 2 s")
except KeyboardInterrupt:
    print("\nstopped: sending 0")
finally:
    t0 = time.time()
    while time.time() - t0 < 2.0:
        send(0)
        time.sleep(0.05)
