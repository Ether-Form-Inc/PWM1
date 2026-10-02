# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ether Form Inc.
"""Unit tests for the arithmetic in bring_up.py. Run: python3 -m unittest tools/test_bring_up.py"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bring_up as b  # noqa: E402


class PrescaleTests(unittest.TestCase):
    def test_a_correct_oscillator_keeps_the_default(self):
        self.assertEqual(b.prescale_for(20000), 121)

    def test_a_fast_oscillator_needs_a_higher_prescale(self):
        # measured on two boards at the default prescale
        self.assertEqual(b.prescale_for(18598), 130)   # 7.5 % fast
        self.assertEqual(b.prescale_for(17838), 136)   # 12 % fast

    def test_a_slow_oscillator_needs_a_lower_prescale(self):
        self.assertLess(b.prescale_for(21000), 121)

    def test_the_result_is_limited_to_what_the_chip_accepts(self):
        self.assertEqual(b.prescale_for(1000), 255)
        self.assertEqual(b.prescale_for(10 ** 7), 3)

    def test_the_calibrated_period_lands_within_one_step_of_the_target(self):
        for period in (17000, 17838, 18598, 19500, 20000, 21500, 23000):
            p = b.prescale_for(period)
            new_period = period * (p + 1) / (b.DEFAULT_PRESCALE + 1)
            self.assertLess(abs(new_period - 20000) / 20000, 0.0045, period)

    def test_no_period_is_an_error(self):
        with self.assertRaises(ValueError):
            b.prescale_for(0)


class IdlePulseTests(unittest.TestCase):
    def test_the_idle_pulse_is_204_counts(self):
        self.assertAlmostEqual(b.expected_high_us(20000), 204 * 20000 / 4096, places=3)
        self.assertAlmostEqual(b.expected_high_us(19973), 204 * 19973 / 4096, places=3)


if __name__ == "__main__":
    unittest.main()
