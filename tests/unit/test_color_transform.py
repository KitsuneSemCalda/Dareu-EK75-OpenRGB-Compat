#!/usr/bin/env python3
"""Unit tests for tools/color_transform.py: pure functions, no OpenRGB, no keyboard.

    python3 tests/unit/test_color_transform.py -v
"""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import color_transform as ct  # noqa: E402


class ParseHex(unittest.TestCase):
    def test_accepts_with_hash(self):
        self.assertEqual(ct.parse_hex("#7aa2f7"), (0x7A, 0xA2, 0xF7))

    def test_accepts_without_hash(self):
        self.assertEqual(ct.parse_hex("7AA2F7"), (0x7A, 0xA2, 0xF7))

    def test_rejects_short_string(self):
        with self.assertRaises(ValueError):
            ct.parse_hex("#abc")

    def test_rejects_non_hex_characters(self):
        with self.assertRaises(ValueError):
            ct.parse_hex("zzzzzz")

    def test_format_hex_round_trips(self):
        self.assertEqual(ct.format_hex((0x7A, 0xA2, 0xF7)), "7aa2f7")


class NoProfile(unittest.TestCase):
    def test_missing_path_is_default_profile(self):
        self.assertEqual(ct.load_profile(None), ct.DEFAULT_PROFILE)

    def test_nonexistent_file_is_default_profile(self):
        self.assertEqual(ct.load_profile("/no/such/file.toml"), ct.DEFAULT_PROFILE)

    def test_default_profile_has_no_brightness(self):
        self.assertIsNone(ct.DEFAULT_PROFILE.brightness)


class NeutralProfile(unittest.TestCase):
    """No calibration configured must not visibly alter the colour."""

    def test_extremes_round_trip(self):
        for hexcolor in ("000000", "ffffff", "ff0000", "00ff00", "0000ff"):
            with self.subTest(hexcolor=hexcolor):
                corrected = ct.transform(hexcolor)
                r1, g1, b1 = ct.parse_hex(hexcolor)
                r2, g2, b2 = ct.parse_hex(corrected)
                for a, b in ((r1, r2), (g1, g2), (b1, b2)):
                    self.assertLessEqual(abs(a - b), 1)

    def test_arbitrary_theme_colour_round_trips(self):
        r1, g1, b1 = ct.parse_hex("7aa2f7")
        r2, g2, b2 = ct.parse_hex(ct.transform("7aa2f7"))
        for a, b in ((r1, r2), (g1, g2), (b1, b2)):
            self.assertLessEqual(abs(a - b), 1)


class LightnessClamp(unittest.TestCase):
    def test_clamps_into_range(self):
        profile = ct.ColorProfile(min_lightness=0.3, max_lightness=0.6)
        corrected = ct.transform("ffffff", profile)  # very light input, forced down
        L, _, _ = ct.hex_to_oklch(corrected)
        self.assertLessEqual(L, 0.6 + 1e-3)

    def test_preserves_hue_within_a_small_tolerance(self):
        before_L, before_C, before_h = ct.hex_to_oklch("7aa2f7")
        profile = ct.ColorProfile(min_lightness=0.3, max_lightness=0.6)
        after_L, after_C, after_h = ct.hex_to_oklch(ct.transform("7aa2f7", profile))
        self.assertAlmostEqual(before_h, after_h, delta=0.02)


class ChromaClampAndGamut(unittest.TestCase):
    def test_max_chroma_desaturates(self):
        profile = ct.ColorProfile(max_chroma=0.02)
        _, before_C, _ = ct.hex_to_oklch("0000ff")
        _, after_C, _ = ct.hex_to_oklch(ct.transform("0000ff", profile))
        self.assertLess(after_C, before_C)
        self.assertLessEqual(after_C, 0.02 + 1e-3)

    def test_output_is_always_a_valid_byte_colour(self):
        profile = ct.ColorProfile(min_lightness=0.9, max_lightness=1.0, max_chroma=5.0)
        corrected = ct.transform("0000ff", profile)
        r, g, b = ct.parse_hex(corrected)
        for value in (r, g, b):
            self.assertGreaterEqual(value, 0)
            self.assertLessEqual(value, 255)


class ChannelGains(unittest.TestCase):
    def test_higher_red_gain_increases_red_channel(self):
        baseline = ct.parse_hex(ct.transform("808080"))
        boosted = ct.parse_hex(ct.transform("808080", ct.ColorProfile(red_gain=1.5)))
        self.assertGreater(boosted[0], baseline[0])
        self.assertEqual(boosted[1], baseline[1])
        self.assertEqual(boosted[2], baseline[2])

    def test_rejects_zero_or_negative_gain(self):
        with self.assertRaises(ct.ColorProfileError):
            ct._validate(ct.ColorProfile(red_gain=0.0), "test")


class Gamma(unittest.TestCase):
    def test_gamma_above_one_brightens_midtone(self):
        baseline = ct.parse_hex(ct.transform("808080"))
        gamma_corrected = ct.parse_hex(ct.transform("808080", ct.ColorProfile(red_gamma=2.0)))
        self.assertGreater(gamma_corrected[0], baseline[0])

    def test_rejects_zero_or_negative_gamma(self):
        with self.assertRaises(ct.ColorProfileError):
            ct._validate(ct.ColorProfile(red_gamma=0.0), "test")


class Brightness(unittest.TestCase):
    def test_default_profile_omits_brightness(self):
        self.assertIsNone(ct.DEFAULT_PROFILE.brightness)

    def test_valid_brightness_accepted(self):
        ct._validate(ct.ColorProfile(brightness=70), "test")  # must not raise

    def test_out_of_range_brightness_rejected(self):
        for value in (-5, 101, 1000):
            with self.subTest(value=value):
                with self.assertRaises(ct.ColorProfileError):
                    ct._validate(ct.ColorProfile(brightness=value), "test")


class ProfileFile(unittest.TestCase):
    def _write(self, text):
        f = tempfile.NamedTemporaryFile(mode="w", suffix=".toml", delete=False)
        f.write(text)
        f.close()
        self.addCleanup(os.unlink, f.name)
        return f.name

    def test_loads_a_well_formed_profile(self):
        path = self._write(
            "[keyboard]\nbrightness = 60\n\n[channels]\nred_gain = 1.2\n"
        )
        profile = ct.load_profile(path)
        self.assertEqual(profile.brightness, 60)
        self.assertEqual(profile.red_gain, 1.2)

    def test_malformed_toml_raises_clear_error(self):
        path = self._write("this is not [ valid toml")
        with self.assertRaises(ct.ColorProfileError):
            ct.load_profile(path)

    def test_out_of_range_value_in_file_raises(self):
        path = self._write("[keyboard]\nbrightness = 500\n")
        with self.assertRaises(ct.ColorProfileError):
            ct.load_profile(path)

    def test_unknown_sections_are_ignored_not_rejected(self):
        path = self._write("[lut]\nred_lut = [0]\n")
        profile = ct.load_profile(path)  # reserved section, must not raise
        self.assertEqual(profile, ct.DEFAULT_PROFILE)


class ResolveHelper(unittest.TestCase):
    def test_resolve_returns_hex_and_brightness(self):
        path = None
        corrected, brightness = ct.resolve("7aa2f7", path)
        self.assertEqual(len(corrected), 6)
        self.assertIsNone(brightness)


if __name__ == "__main__":
    unittest.main()
