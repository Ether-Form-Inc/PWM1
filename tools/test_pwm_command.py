# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ether Form Inc.
"""Unit tests for the parts of pwm_command.py that need no hardware.
Run with the Python that has dronecan installed:  python3 -m unittest tools/test_pwm_command.py"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dronecan.transport as transport  # noqa: E402
import pwm_command as p  # noqa: E402


def roundtrip(message):
    """Encodes a message into CAN frames and decodes it again, as the node would receive it."""
    tr = transport.Transfer(transfer_id=3, source_node_id=50, payload=message, transfer_priority=8)
    manager = transport.TransferManager()
    for frame in tr.to_frames():
        done = manager.receive_frame(transport.Frame(frame.message_id, bytes(frame.bytes)))
        if done:
            out = transport.Transfer()
            out.from_frames(done)
            return out.payload
    raise AssertionError("the message did not come out of the frames")


class ParsePairsTests(unittest.TestCase):
    def test_motor_values(self):
        self.assertEqual(p.parse_pairs(["0=300", "2=8191"], "motor"), {0: 300, 2: 8191})

    def test_all_means_every_output(self):
        self.assertEqual(p.parse_pairs(["all=0"], "motor"), {0: 0, 1: 0, 2: 0, 3: 0})

    def test_a_later_pair_overrides_all(self):
        self.assertEqual(p.parse_pairs(["all=100", "1=500"], "motor")[1], 500)

    def test_bad_motor_values_are_refused(self):
        for bad in ("0=-1", "0=8192", "0=1.5", "0=abc", "4=10", "x=1", "0"):
            with self.assertRaises(ValueError, msg=bad):
                p.parse_pairs([bad], "motor")

    def test_servo_ranges(self):
        self.assertEqual(p.parse_pairs(["1=1500"], "servo-us"), {1: 1500.0})
        self.assertEqual(p.parse_pairs(["0=-0.5"], "servo-unitless"), {0: -0.5})
        for kind, bad in (("servo-us", "0=0"), ("servo-us", "0=2049"), ("servo-unitless", "0=1.1"),
                          ("servo-unitless", "0=-2")):
            with self.assertRaises(ValueError, msg=bad):
                p.parse_pairs([bad], kind)


class MessageTests(unittest.TestCase):
    def test_motor_values_land_at_offset_plus_output(self):
        msg = roundtrip(p.build_message("motor", {0: 300, 2: 1000}, 0))
        self.assertEqual(list(msg.cmd), [300, 0, 1000])
        msg = roundtrip(p.build_message("motor", {1: 500}, 4))
        self.assertEqual(list(msg.cmd), [0, 0, 0, 0, 0, 500])

    def test_motor_message_too_long_is_refused(self):
        with self.assertRaises(ValueError):
            p.build_message("motor", {3: 1}, 18)

    def test_servo_pulse_width_is_a_pwm_command(self):
        msg = roundtrip(p.build_message("servo-us", {1: 1500, 3: 1200}, 0))
        got = {c.actuator_id: (c.command_type, c.command_value) for c in msg.commands}
        self.assertEqual(got, {1: (4, 1500.0), 3: (4, 1200.0)})

    def test_servo_unitless_and_offset(self):
        msg = roundtrip(p.build_message("servo-unitless", {0: -0.5}, 2))
        c = msg.commands[0]
        self.assertEqual((c.actuator_id, c.command_type), (2, 0))
        self.assertAlmostEqual(c.command_value, -0.5, places=3)


class BusInspectionTests(unittest.TestCase):
    @staticmethod
    def message_id(priority, type_id, source):
        return (priority << 24) | (type_id << 8) | source

    def test_the_nodes_own_status_gives_its_mode(self):
        frames = [(self.message_id(1, 341, 10), b"")]
        self.assertEqual(p.inspect_bus(frames), (set(), 1))
        frames = [(self.message_id(20, 341, 10), b"")]
        self.assertEqual(p.inspect_bus(frames), (set(), 20))

    def test_another_command_sender_is_detected(self):
        frames = [(self.message_id(8, 1030, 99), b""), (self.message_id(8, 1010, 51), b"")]
        senders, _ = p.inspect_bus(frames)
        self.assertEqual(senders, {(1030, 99), (1010, 51)})

    def test_our_own_frames_and_services_are_ignored(self):
        frames = [(self.message_id(8, 1030, 50), b""),                  # us
                  (self.message_id(20, 1030, 7) | 0x80, b"")]           # a service frame that looks similar
        self.assertEqual(p.inspect_bus(frames), (set(), None))


class ArgumentTests(unittest.TestCase):
    def parse(self, *argv):
        return p.parse_args(list(argv))

    def test_a_good_command(self):
        a = self.parse("--motor", "0=300", "--duration", "5")
        self.assertEqual((a.kind, a.values, a.duration), ("motor", {0: 300}, 5.0))

    def test_high_motor_values_need_allow_high(self):
        with self.assertRaises(SystemExit):
            self.parse("--motor", "0=3000")
        self.assertEqual(self.parse("--motor", "0=3000", "--allow-high").values, {0: 3000})

    def test_the_command_types_cannot_be_mixed(self):
        with self.assertRaises(SystemExit):
            self.parse("--motor", "0=1", "--servo-us", "0=1500")

    def test_bad_offset_and_rate(self):
        for argv in (["--motor", "0=1", "--offset", "8"], ["--motor", "0=1", "--rate", "0"]):
            with self.assertRaises(SystemExit):
                self.parse(*argv)


if __name__ == "__main__":
    unittest.main()
