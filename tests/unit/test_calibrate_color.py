#!/usr/bin/env python3
"""Unit tests for tools/calibrate_color.py: sequence generation and Ctrl+C handling.

Never calls the real OpenRGB binary: apply() is monkeypatched throughout.

    python3 tests/unit/test_calibrate_color.py -v
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import calibrate_color as cc  # noqa: E402


class Sequences(unittest.TestCase):
    def test_red_sequence_only_touches_the_red_channel(self):
        for _, hexcolor in cc.channel_sequence("red"):
            self.assertEqual(hexcolor[2:], "0000")

    def test_channel_sequence_is_monotonically_increasing(self):
        values = [int(hexcolor[4:6], 16) for _, hexcolor in cc.channel_sequence("blue")]
        self.assertEqual(values, sorted(values))
        self.assertEqual(values[-1], 0xFF)

    def test_gray_sequence_has_equal_channels(self):
        for _, hexcolor in cc.gray_sequence():
            r, g, b = hexcolor[0:2], hexcolor[2:4], hexcolor[4:6]
            self.assertEqual(r, g)
            self.assertEqual(g, b)

    def test_palette_covers_primaries_and_secondaries(self):
        names = [name for name, _ in cc.palette_sequence()]
        for expected in ("red", "green", "blue", "cyan", "magenta", "yellow", "white", "gray"):
            self.assertIn(expected, names)

    def test_brightness_sequence_holds_colour_constant_and_sweeps_level(self):
        samples = cc.brightness_sequence("aabbcc")
        colours = {hexcolor for _, hexcolor, _ in samples}
        levels = [level for _, _, level in samples]
        self.assertEqual(colours, {"aabbcc"})
        self.assertEqual(levels, sorted(levels))
        self.assertEqual(levels[0], 10)
        self.assertEqual(levels[-1], 100)


class MainLoop(unittest.TestCase):
    def setUp(self):
        self.calls = []
        self._real_apply = cc.apply
        self._real_sleep = cc.time.sleep
        cc.apply = lambda device, hexcolor, brightness=None, mode="Static": \
            self.calls.append((device, hexcolor, brightness))
        cc.time.sleep = lambda seconds: None
        self._real_access = os.access
        os.access = lambda *a, **k: True

    def tearDown(self):
        cc.apply = self._real_apply
        cc.time.sleep = self._real_sleep
        os.access = self._real_access

    def test_runs_every_sample_in_the_sequence(self):
        rc = cc.main(["gray", "--interval", "0"])
        self.assertEqual(rc, 0)
        self.assertEqual(len(self.calls), len(cc.GRAY_STEPS))

    def test_ctrl_c_stops_cleanly_with_exit_code_130(self):
        def raise_interrupt(*a, **k):
            raise KeyboardInterrupt

        cc.apply = raise_interrupt
        rc = cc.main(["red", "--interval", "0"])
        self.assertEqual(rc, 130)

    def test_missing_openrgb_binary_is_a_clear_error_not_a_crash(self):
        os.access = lambda *a, **k: False
        rc = cc.main(["red", "--interval", "0"])
        self.assertEqual(rc, 1)
        self.assertEqual(self.calls, [])


if __name__ == "__main__":
    unittest.main()
