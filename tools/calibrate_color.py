#!/usr/bin/env python3
"""Send predictable colour/brightness sequences to the EK75, for calibration study.

Usage:
  calibrate_color.py red|green|blue|gray [--interval SECONDS] [--device NAME]
  calibrate_color.py palette [--interval SECONDS] [--device NAME]
  calibrate_color.py brightness [--color RRGGBB] [--interval SECONDS] [--device NAME]

Each sample is applied through the OpenRGB build this project installs (or $OPENRGB),
with the value printed before it is sent, so it can be watched and logged by hand.
Nothing here computes gamma, gain or a LUT: it only produces raw samples. Feed what
you observe into ~/.config/dareu-ek75/color-profile.toml (see docs/THEME_COLOR.md).

Ctrl+C stops after the sample in flight and leaves the keyboard as it is.
"""
import argparse
import os
import subprocess
import sys
import time

DEFAULT_DEVICE = "Dareu EK75"

CHANNEL_STEPS = (0x10, 0x20, 0x30, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xE0, 0xFF)
GRAY_STEPS = (0x10, 0x20, 0x40, 0x80, 0xC0, 0xFF)
BRIGHTNESS_LEVELS = (10, 20, 30, 40, 50, 60, 70, 80, 90, 100)

PALETTE = (
    ("red", "ff0000"),
    ("green", "00ff00"),
    ("blue", "0000ff"),
    ("cyan", "00ffff"),
    ("magenta", "ff00ff"),
    ("yellow", "ffff00"),
    ("white", "ffffff"),
    ("gray", "808080"),
)


def openrgb_path():
    return os.environ.get("OPENRGB", os.path.expanduser("~/.local/lib/dareu-ek75/openrgb"))


def apply(device, hexcolor, brightness=None, mode="Static"):
    cmd = [openrgb_path(), "--noautoconnect", "-d", device, "-m", mode, "-c", hexcolor]
    if brightness is not None:
        cmd += ["-b", str(brightness)]
    subprocess.run(cmd, check=True)


def channel_sequence(channel):
    """[(label, hex), ...] for a single-channel ramp: red/green/blue."""
    index = {"red": 0, "green": 1, "blue": 2}[channel]
    out = []
    for step in CHANNEL_STEPS:
        rgb = [0, 0, 0]
        rgb[index] = step
        out.append((f"{channel} {step:#04x}", "{:02x}{:02x}{:02x}".format(*rgb)))
    return out


def gray_sequence():
    return [(f"gray {step:#04x}", f"{step:02x}{step:02x}{step:02x}") for step in GRAY_STEPS]


def palette_sequence():
    return [(name, hexcolor) for name, hexcolor in PALETTE]


def brightness_sequence(hexcolor):
    return [(f"brightness {level}% @ #{hexcolor}", hexcolor, level) for level in BRIGHTNESS_LEVELS]


def run_sequence(samples, device, interval):
    for sample in samples:
        if len(sample) == 3:
            label, hexcolor, brightness = sample
        else:
            label, hexcolor = sample
            brightness = None

        print(label)
        apply(device, hexcolor, brightness)
        time.sleep(interval)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sequence", choices=["red", "green", "blue", "gray", "palette", "brightness"])
    parser.add_argument("--interval", type=float, default=3.0,
                         help="seconds to wait between samples (default: %(default)s)")
    parser.add_argument("--device", default=DEFAULT_DEVICE,
                         help="OpenRGB device name (default: %(default)r)")
    parser.add_argument("--color", default="ffffff",
                         help="reference colour for the brightness sequence (default: %(default)s)")
    args = parser.parse_args(argv)

    if not os.access(openrgb_path(), os.X_OK):
        print(f"{openrgb_path()} not found or not executable; run tools/build.sh and "
              f"tools/install-launcher.sh, or set $OPENRGB", file=sys.stderr)
        return 1

    if args.sequence in ("red", "green", "blue"):
        samples = channel_sequence(args.sequence)
    elif args.sequence == "gray":
        samples = gray_sequence()
    elif args.sequence == "palette":
        samples = palette_sequence()
    else:
        samples = brightness_sequence(args.color.lstrip("#"))

    try:
        run_sequence(samples, args.device, args.interval)
    except KeyboardInterrupt:
        print("\nstopped")
        return 130

    return 0


if __name__ == "__main__":
    sys.exit(main())
