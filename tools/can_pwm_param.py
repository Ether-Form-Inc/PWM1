#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ether Form Inc.
"""Reads and sets the can_to_pwm parameters over CAN through the serial USB-CAN adapter (needs: pip install dronecan
pyserial; the protocol is in can_link.py).

Usage: can_pwm_param.py [-d /dev/ttyUSB0] NODE_ID command [name] [value]

  list                 show every parameter
  get NAME             show one parameter
  set NAME VALUE       store a value (applied at once, except NODE_ID which applies after a restart)
  set-range KIND OUT MIN MAX
                       set both pulse width limits (us) of one output, KIND is servo or motor, OUT is 0 to 3.
                       The limits are written in an order that never leaves max <= min.
  measure OUT          time PWM output OUT (0 to 3) with the node's timer and show the period and high time (bench
                       use: the node refuses once it has received a command since it started)
  erase                clear every stored value, back to the defaults
  restart              restart the node (applies a new NODE_ID)

Parameters: NODE_ID (1-127, 0 = default 10); the pulse width limits in us (1-20000, 0 = default)
SERVO_MIN_US_0..3, SERVO_MAX_US_0..3, MOTOR_MIN_US_0..3, MOTOR_MAX_US_0..3; MOTOR_TIMEOUT_MS (100-60000 ms, 0 = default
5000); MOTOR_TIMEOUT_ACTION (1 = hold the minimum pulse instead of switching off); PWM_PRESCALE (3-255, 0 = default 121);
MOTOR_ARM_MS (100-10000 ms, 0 = off). PWM_MEASURE, PWM_PERIOD_US and PWM_HIGH_US are the bench measurement.
"""
import argparse
import sys

from can_link import CanLink, LinkError

ap = argparse.ArgumentParser(add_help=False)
ap.add_argument("-d", "--device", default="/dev/ttyUSB0")
ap.add_argument("node", type=int)
ap.add_argument("command", choices=["list", "get", "set", "set-range", "measure", "erase", "restart"])
ap.add_argument("args", nargs="*")
args = ap.parse_args()


def show(p):
    print(f"{p.name:16s} = {p.value:6d}   default {p.default:5d}   min {p.min}   max {p.max}")


def main():
    link = CanLink(args.device, args.node)
    cmd, a = args.command, args.args
    if cmd == "list":
        for p in link.params():
            show(p)
    elif cmd == "get":
        if len(a) != 1:
            sys.exit("get needs a parameter name")
        show(link.get(a[0]))
    elif cmd == "set":
        if len(a) != 2:
            sys.exit("set needs a name and a value")
        wanted = int(a[1])
        p = link.set(a[0], wanted)
        show(p)
        if wanted != 0 and p.value != wanted:
            print(f"  not applied: the board kept {p.value} (value out of range, or max would not be above min)")
    elif cmd == "set-range":
        if len(a) != 4 or a[0] not in ("servo", "motor") or a[1] not in "0123":
            sys.exit("set-range needs: servo|motor OUTPUT(0-3) MIN MAX")
        kind, out, lo, hi = a[0].upper(), a[1], int(a[2]), int(a[3])
        if not 0 < lo < hi <= 20000:
            sys.exit("need 0 < MIN < MAX <= 20000")
        cur_lo = link.get(f"{kind}_MIN_US_{out}").value
        # Raise the maximum first when moving up, lower the minimum first when moving down, so max > min throughout
        order = [("MAX", hi), ("MIN", lo)] if lo >= cur_lo else [("MIN", lo), ("MAX", hi)]
        for which, v in order:
            p = link.set(f"{kind}_{which}_US_{out}", v)
            show(p)
            if p.value != v:
                print(f"  not applied: the board kept {p.value}")
    elif cmd == "measure":
        if len(a) != 1 or a[0] not in ("0", "1", "2", "3"):
            sys.exit("measure needs an output number 0 to 3")
        period, high = link.measure(int(a[0]))
        print(f"output {a[0]}: period {period} us ({1e6 / period:.2f} Hz), high time {high} us")
    elif cmd == "erase":
        print("erase:", "ok" if link.erase_all() else "failed")
    elif cmd == "restart":
        print("restart:", "accepted" if link.restart() else "refused")


try:
    main()
except LinkError as e:
    sys.exit(str(e))
