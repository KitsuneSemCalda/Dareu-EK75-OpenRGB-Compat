#!/usr/bin/env python3
"""Perceptual colour transform for the EK75 theme integration (stdlib only).

This is the *theme calibration* layer, not the hardware protocol: it turns a
logical theme colour (whatever Omarchy's colors.toml says) into the RGB that
looks closest to it on this keyboard's LEDs, plus an optional physical
brightness. The driver still receives a plain RGB and sends exactly that; all
colour policy lives here, in tools/, not in src/DareuEK75Controller/.

Pipeline: sRGB -> linear RGB -> OKLab/OKLCh -> clamp Lightness -> clamp Chroma
-> gamut-map back into sRGB (hue held fixed) -> per-channel gain/gamma -> 8-bit
RGB. See docs/THEME_COLOR.md for the reasoning and docs/RESEARCH.md for the
wire protocol.

CLI:
  color_transform.py apply --hex RRGGBB [--profile PATH]
      Prints "<corrected-hex> [brightness]" on success, exit 0.
      Exit 1 with a message on stderr for an invalid colour or profile.
"""
import argparse
import math
import os
import sys
from dataclasses import dataclass

try:
    import tomllib
except ImportError:  # pragma: no cover
    sys.exit("color_transform.py needs Python 3.11+ (stdlib tomllib)")

import re

_HEX_RE = re.compile(r"^#?([0-9a-fA-F]{6})$")


def parse_hex(text):
    """'RRGGBB' or '#RRGGBB' -> (r, g, b) ints 0-255. Raises ValueError."""
    match = _HEX_RE.match(text.strip())
    if not match:
        raise ValueError(f"invalid colour '{text}': expected RRGGBB or #RRGGBB")
    digits = match.group(1)
    return tuple(int(digits[i:i + 2], 16) for i in (0, 2, 4))


def format_hex(rgb):
    r, g, b = rgb
    return f"{r:02x}{g:02x}{b:02x}"


def _clamp01(v):
    return 0.0 if v < 0.0 else 1.0 if v > 1.0 else v


# --- sRGB <-> linear ---------------------------------------------------

def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def linear_to_srgb(c):
    c = _clamp01(c)
    return c * 12.92 if c <= 0.0031308 else 1.055 * (c ** (1 / 2.4)) - 0.055


# --- linear sRGB <-> OKLab (Bjorn Ottosson, https://bottosson.github.io/posts/oklab/) --

def _cbrt(x):
    return math.copysign(abs(x) ** (1 / 3), x)


def linear_srgb_to_oklab(r, g, b):
    l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b
    m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b
    s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b

    l_, m_, s_ = _cbrt(l), _cbrt(m), _cbrt(s)

    L = 0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_
    a = 1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_
    b2 = 0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_
    return L, a, b2


def oklab_to_linear_srgb(L, a, b):
    l_ = L + 0.3963377774 * a + 0.2158037573 * b
    m_ = L - 0.1055613458 * a - 0.0638541728 * b
    s_ = L - 0.0894841775 * a - 1.2914855480 * b

    l, m, s = l_ ** 3, m_ ** 3, s_ ** 3

    r = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s
    g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s
    b2 = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s
    return r, g, b2


def hex_to_oklch(hex_str):
    r, g, b = parse_hex(hex_str)
    lr, lg, lb = (srgb_to_linear(v / 255.0) for v in (r, g, b))
    L, a, b2 = linear_srgb_to_oklab(lr, lg, lb)
    return L, math.hypot(a, b2), math.atan2(b2, a)


def _in_gamut(L, C, h):
    a, b = C * math.cos(h), C * math.sin(h)
    r, g, bl = oklab_to_linear_srgb(L, a, b)
    eps = 1e-6
    return all(-eps <= v <= 1 + eps for v in (r, g, bl))


def _max_in_gamut_chroma(L, h, C):
    """Largest chroma <= C that maps to a real sRGB colour, hue and lightness fixed."""
    if C <= 0.0 or _in_gamut(L, C, h):
        return C

    lo, hi = 0.0, C
    for _ in range(24):
        mid = (lo + hi) / 2
        if _in_gamut(L, mid, h):
            lo = mid
        else:
            hi = mid
    return lo


def oklch_to_hex(L, C, h, gains=(1.0, 1.0, 1.0), gammas=(1.0, 1.0, 1.0)):
    C = _max_in_gamut_chroma(L, h, max(C, 0.0))
    a, b = C * math.cos(h), C * math.sin(h)
    lr, lg, lb = oklab_to_linear_srgb(L, a, b)
    srgb = [linear_to_srgb(v) for v in (lr, lg, lb)]

    out = []
    for v, gain, gamma in zip(srgb, gains, gammas):
        v = _clamp01(v * gain)
        if gamma != 1.0:
            v = v ** (1.0 / gamma)
        out.append(_clamp01(v))

    rgb255 = tuple(max(0, min(255, round(v * 255))) for v in out)
    return format_hex(rgb255)


# --- calibration profile -------------------------------------------------

class ColorProfileError(Exception):
    pass


@dataclass
class ColorProfile:
    """Neutral by default: every field here means 'do not alter the colour'."""
    brightness: "int | None" = None
    min_lightness: float = 0.0
    max_lightness: float = 1.0
    max_chroma: "float | None" = None
    red_gain: float = 1.0
    green_gain: float = 1.0
    blue_gain: float = 1.0
    red_gamma: float = 1.0
    green_gamma: float = 1.0
    blue_gamma: float = 1.0


DEFAULT_PROFILE = ColorProfile()


def default_profile_path():
    base = os.environ.get("XDG_CONFIG_HOME") or os.path.join(os.path.expanduser("~"), ".config")
    return os.path.join(base, "dareu-ek75", "color-profile.toml")


def _validate(profile, path):
    def fail(msg):
        raise ColorProfileError(f"invalid color profile '{path}': {msg}")

    if profile.brightness is not None and not (0 <= profile.brightness <= 100):
        fail(f"keyboard.brightness must be 0-100, got {profile.brightness}")
    if not (0.0 <= profile.min_lightness <= 1.0):
        fail("color.min_lightness must be between 0 and 1")
    if not (0.0 <= profile.max_lightness <= 1.0):
        fail("color.max_lightness must be between 0 and 1")
    if profile.min_lightness > profile.max_lightness:
        fail("color.min_lightness must be <= color.max_lightness")
    if profile.max_chroma is not None and profile.max_chroma <= 0:
        fail("color.max_chroma must be > 0")
    for name, value in (("red_gain", profile.red_gain), ("green_gain", profile.green_gain),
                         ("blue_gain", profile.blue_gain)):
        if value <= 0:
            fail(f"channels.{name} must be > 0")
    for name, value in (("red", profile.red_gamma), ("green", profile.green_gamma),
                         ("blue", profile.blue_gamma)):
        if value <= 0:
            fail(f"gamma.{name} must be > 0")


def load_profile(path):
    """Missing file -> DEFAULT_PROFILE. Malformed or out-of-range -> ColorProfileError."""
    if path is None or not os.path.isfile(path):
        return DEFAULT_PROFILE

    try:
        with open(path, "rb") as f:
            data = tomllib.load(f)
    except tomllib.TOMLDecodeError as e:
        raise ColorProfileError(f"invalid color profile '{path}': {e}") from e

    keyboard = data.get("keyboard", {})
    color = data.get("color", {})
    channels = data.get("channels", {})
    gamma = data.get("gamma", {})
    # data.get("lut", {}) is reserved for a future red_lut/green_lut/blue_lut table
    # (see docs/THEME_COLOR.md); intentionally not read yet.

    profile = ColorProfile(
        brightness=keyboard.get("brightness"),
        min_lightness=color.get("min_lightness", 0.0),
        max_lightness=color.get("max_lightness", 1.0),
        max_chroma=color.get("max_chroma"),
        red_gain=channels.get("red_gain", 1.0),
        green_gain=channels.get("green_gain", 1.0),
        blue_gain=channels.get("blue_gain", 1.0),
        red_gamma=gamma.get("red", 1.0),
        green_gamma=gamma.get("green", 1.0),
        blue_gamma=gamma.get("blue", 1.0),
    )
    _validate(profile, path)
    return profile


def transform(hex_str, profile=None):
    """Apply a ColorProfile to a theme colour. profile=None means the neutral default."""
    profile = profile or DEFAULT_PROFILE
    L, C, h = hex_to_oklch(hex_str)

    L = min(max(L, profile.min_lightness), profile.max_lightness)
    if profile.max_chroma is not None:
        C = min(C, profile.max_chroma)

    gains = (profile.red_gain, profile.green_gain, profile.blue_gain)
    gammas = (profile.red_gamma, profile.green_gamma, profile.blue_gamma)
    return oklch_to_hex(L, C, h, gains, gammas)


def resolve(hex_str, profile_path):
    """Load the profile at profile_path and return (corrected_hex, brightness_or_None)."""
    profile = load_profile(profile_path)
    return transform(hex_str, profile), profile.brightness


def _cmd_apply(args):
    parse_hex(args.hex)  # fail fast with a clear message before touching the profile
    path = args.profile or default_profile_path()
    corrected, brightness = resolve(args.hex, path)
    print(corrected, "" if brightness is None else brightness)
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_apply = sub.add_parser("apply", help="resolve the corrected colour (+ brightness) for a theme colour")
    p_apply.add_argument("--hex", required=True, help="theme colour, RRGGBB or #RRGGBB")
    p_apply.add_argument("--profile", default=None,
                          help=f"colour profile TOML path (default: {default_profile_path()})")

    args = parser.parse_args(argv)

    try:
        if args.cmd == "apply":
            return _cmd_apply(args)
    except (ValueError, ColorProfileError) as e:
        print(str(e), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
