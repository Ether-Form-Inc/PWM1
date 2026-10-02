#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ether Form Inc.
"""Holds can_to_pwm output values by sending DroneCAN commands through the serial USB-CAN adapter, until Ctrl-C (or
--duration). REMOVE THE PROPELLERS before commanding motors.

Motor mode (J5 row 1 open) takes RawCommand; servo mode (row 1 fitted) takes ArrayCommand. Outputs are numbered 0 to 3 on
the board; --offset N adds the board's J5 actuator ID offset (the DroneCAN actuator id is offset + output).

  pwm_command.py --motor 0=300 2=300           RawCommand values (0 to 8191; 8191 is the maximum pulse) on outputs 0, 2
  pwm_command.py --motor all=0                 all four outputs at the minimum pulse (zero throttle)
  pwm_command.py --servo-us 1=1500 3=1200      servo pulse widths in microseconds (exact up to 2048 us)
  pwm_command.py --servo-unitless 0=-0.5       servo positions from -1 to +1 (mapped onto the servo limits)

Options: --duration SECONDS, --rate HZ (default 20), --offset N, --allow-high (motor values above 2048, about 25 %),
--force (send even if another sender is on the bus), --no-wait.

Safety: the adapter is opened exclusively, so a second program using it (a leftover motor_ramp.py, another pwm_command.py)
is refused. Before sending it also listens to the bus for about 1.5 s and refuses to start if another node is sending
commands (a flight controller or a second adapter; two senders interleave their values on the outputs), and it warns if
the node is in the other mode. Motor values above
2048 need --allow-high. A 3 s countdown gives you time to stop. On exit, motors are sent 0 for one second; servos keep the
last value, since a servo has no safe stop position, so send the position you want last.
"""
import argparse
import sys
import time

import dronecan

NODE = 10
RAW_COMMAND_ID = 1030
ARRAY_COMMAND_ID = 1010
NODE_STATUS_ID = 341
MOTOR_SAFE_MAX = 2048
MOTOR_MAX = 8191


def parse_pairs(tokens, kind):
    """['0=300', 'all=0'] -> {0: 300.0, ...}. Raises ValueError with a message for the user."""
    out = {}
    for t in tokens:
        if "=" not in t:
            raise ValueError("expected OUTPUT=VALUE, got %r" % t)
        k, v = t.split("=", 1)
        try:
            value = float(v)
        except ValueError:
            raise ValueError("%r is not a number" % v)
        if k.lower() == "all":
            outputs = [0, 1, 2, 3]
        elif k in ("0", "1", "2", "3"):
            outputs = [int(k)]
        else:
            raise ValueError("output must be 0 to 3 or all, got %r" % k)
        if kind == "motor":
            if value != int(value) or not 0 <= value <= MOTOR_MAX:
                raise ValueError("a motor value is a whole number from 0 to %d (negative values are not supported), "
                                 "got %s" % (MOTOR_MAX, v))
        elif kind == "servo-us":
            if not 0 < value <= 2048:
                raise ValueError("a servo pulse width is 1 to 2048 us (the command carries a half precision float), "
                                 "got %s" % v)
        else:
            if not -1.0 <= value <= 1.0:
                raise ValueError("a unitless servo value is -1 to +1, got %s" % v)
        for o in outputs:
            out[o] = int(value) if kind == "motor" else value
    if not out:
        raise ValueError("no outputs given")
    return out


def build_message(kind, values, offset):
    """The DroneCAN message that sets `values` ({output: value}) on a board with actuator ID offset `offset`."""
    if kind == "motor":
        size = offset + max(values) + 1
        if size > 20:
            raise ValueError("RawCommand carries at most 20 values; output %d with offset %d does not fit" %
                             (max(values), offset))
        cmd = [0] * size
        for o, v in values.items():
            cmd[offset + o] = int(v)
        return dronecan.uavcan.equipment.esc.RawCommand(cmd=cmd)
    command_type = 4 if kind == "servo-us" else 0   # COMMAND_TYPE_PWM, COMMAND_TYPE_UNITLESS
    commands = [dronecan.uavcan.equipment.actuator.Command(actuator_id=offset + o, command_type=command_type,
                                                          command_value=float(v)) for o, v in sorted(values.items())]
    return dronecan.uavcan.equipment.actuator.ArrayCommand(commands=commands)


def parse_args(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-d", "--device", default="/dev/ttyUSB0")
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--motor", nargs="+", metavar="OUT=VALUE")
    group.add_argument("--servo-us", nargs="+", metavar="OUT=US", dest="servo_us")
    group.add_argument("--servo-unitless", nargs="+", metavar="OUT=VALUE", dest="servo_unitless")
    ap.add_argument("--offset", type=int, default=0, help="the board's J5 actuator ID offset (0 to 7)")
    ap.add_argument("--duration", type=float, help="seconds to send for (default: until Ctrl-C)")
    ap.add_argument("--rate", type=float, default=20.0, help="messages per second (default 20)")
    ap.add_argument("--allow-high", action="store_true", help="allow motor values above %d" % MOTOR_SAFE_MAX)
    ap.add_argument("--force", action="store_true", help="send even if another command sender is on the bus")
    ap.add_argument("--no-wait", action="store_true", help="skip the 3 s countdown")
    args = ap.parse_args(argv)
    if args.motor:
        args.kind, tokens = "motor", args.motor
    elif args.servo_us:
        args.kind, tokens = "servo-us", args.servo_us
    else:
        args.kind, tokens = "servo-unitless", args.servo_unitless
    try:
        args.values = parse_pairs(tokens, args.kind)
        if not 0 <= args.offset <= 7:
            raise ValueError("--offset is the J5 offset, 0 to 7")
        if not 1 <= args.rate <= 50:
            raise ValueError("--rate must be 1 to 50 Hz")
        if args.kind == "motor" and max(args.values.values()) > MOTOR_SAFE_MAX and not args.allow_high:
            raise ValueError("motor value %d is above %d (about 25 %%): add --allow-high if you mean it" %
                             (max(args.values.values()), MOTOR_SAFE_MAX))
        build_message(args.kind, args.values, args.offset)   # catches a message that does not fit
    except ValueError as e:
        ap.error(str(e))
    return args


def inspect_bus(frames, our_id=50):
    """From raw frames: (other command senders seen, the node's NodeStatus priority or None)."""
    senders, priority = set(), None
    for can_id, _ in frames:
        if can_id & 0x80:                       # a service frame
            continue
        type_id, source = (can_id >> 8) & 0xFFFF, can_id & 0x7F
        if type_id in (RAW_COMMAND_ID, ARRAY_COMMAND_ID) and source != our_id:
            senders.add((type_id, source))
        if type_id == NODE_STATUS_ID and source == NODE:
            priority = (can_id >> 24) & 0x1F
    return senders, priority


def main():
    args = parse_args()
    from can_link import CanLink, LinkError
    try:
        link = CanLink(args.device, NODE)
    except LinkError as e:
        sys.exit(str(e))
    try:
        print("listening to the bus for 1.5 s ...", flush=True)
        senders, priority = inspect_bus(link.read_frames(1.5))
        if senders and not args.force:
            who = ", ".join("%s from node %d" % ("RawCommand" if t == RAW_COMMAND_ID else "ArrayCommand", s)
                            for t, s in sorted(senders))
            sys.exit("another command sender is active on the bus (%s). Stop it first (a flight controller or a second "
                     "adapter), or use --force." % who)
        if priority is None:
            print("note: no NodeStatus from node %d was seen, so the board may not be connected" % NODE)
        else:
            mode = "motor" if priority < 10 else "servo"
            print("node %d is in %s mode (NodeStatus priority %d)" % (NODE, mode, priority))
            if (args.kind == "motor") != (mode == "motor"):
                print("WARNING: you are sending %s commands to a node in %s mode: it ignores them. J5 row 1 open "
                      "is motor mode, fitted is servo mode; the mode is read at power-up." % (args.kind, mode))
        message = build_message(args.kind, args.values, args.offset)
        desc = ", ".join("output %d = %s" % (o, ("%g" % v)) for o, v in sorted(args.values.items()))
        print("sending %s: %s%s at %g Hz" % (args.kind, desc, (" (actuator ids + %d)" % args.offset) if args.offset else "",
                                             args.rate))
        if args.kind == "motor":
            print("REMOVE THE PROPELLERS. Ctrl-C stops and sends 0.")
        if not args.no_wait:
            for n in (3, 2, 1):
                print("  starting in %d ..." % n, flush=True)
                time.sleep(1)
        end = time.time() + args.duration if args.duration else None
        sent, period = 0, 1.0 / args.rate
        next_time = time.time()
        try:
            while end is None or time.time() < end:
                link.broadcast(message)
                sent += 1
                next_time += period
                time.sleep(max(0.0, next_time - time.time()))
        except KeyboardInterrupt:
            print()
        print("stopped after %d messages" % sent)
    finally:
        if args.kind == "motor":
            stop = build_message("motor", {o: 0 for o in args.values}, args.offset)
            t_end = time.time() + 1.0
            while time.time() < t_end:
                link.broadcast(stop)
                time.sleep(0.05)
            print("sent 0 to the motor outputs")
        else:
            print("servo outputs keep their last value")
        link.close()


if __name__ == "__main__":
    main()
