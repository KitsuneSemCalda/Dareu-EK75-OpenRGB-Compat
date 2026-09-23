# Theme colour calibration

This is about the *Omarchy integration layer* in `tools/`, not the driver. Two different
things get called "colour" in this project and they must stay separate:

| | Lives in | Knows about |
|---|---|---|
| **Hardware protocol** | `src/DareuEK75Controller/` | HID packets, regions, effects. If a client asks for `RGB(122,162,247)`, that is exactly what goes to the keyboard. |
| **Theme colour calibration** | `tools/` | Omarchy, OKLCH, gamma, gain, LUTs. Decides *what RGB to ask for* so the keyboard's LEDs look closer to the theme's intent. |

The driver has no idea Omarchy exists, and never will. Everything below happens before
`openrgb` is invoked.

```
Omarchy theme
      |  keyboard.rgb, else COLOR_KEY (colors.toml), else error
      v
resolved theme colour (sRGB hex)
      |  tools/color_transform.py, only if a colour profile file exists
      v
corrected colour + optional physical brightness
      v
openrgb -c <corrected> [-b <brightness>]   (no -m: keeps whatever mode is already active)
      v
src/DareuEK75Controller/ (unmodified, hardware only)
```

No `-m` is passed on purpose. OpenRGB's own CLI falls back to the device's current mode when
`--mode` is omitted (`ParseMode()` in its `cli.cpp`, `GetActiveMode()`), which for this driver is
whatever the firmware was already showing when it was detected. A theme switch therefore recolours
Raindrop, Breathing, or whatever is running, instead of replacing it with a flat `Static` colour.
A mode with no colour slot (most of the rainbow-cycling ones) simply keeps animating, untouched.

## Why monitor RGB != LED RGB

`#7AA2F7` on a calibrated monitor and `#7AA2F7` sent to the EK75's backlight LEDs are not
guaranteed to look the same. Different LEDs, different diffusers, different drive
electronics: the mapping from a command byte to visible light is specific to this
hardware and is not sRGB. Sending the theme colour unmodified is a reasonable default
(and is exactly what happens with no profile configured), but it is not colour-accurate.

Getting closer needs two things this project cannot supply on its own: measurements
from *your* keyboard (see [Calibration](#calibration) below) and, ideally, a colorimeter.
Nothing here claims colorimetric accuracy without one.

## Resolving the theme colour

`tools/apply-theme.sh [theme-name]` (defaults to the current Omarchy theme) resolves a
colour in this order:

1. **`keyboard.rgb`** in the theme's directory, if the theme provides one. A plain text
   file with a single colour, `RRGGBB` or `#RRGGBB` (this follows the same convention
   Omarchy themes already use for other per-app files, e.g. a `waybar.css`; it is not a
   second theme format).
2. Otherwise, **`$COLOR_KEY`** (default: `accent`) read from the theme's `colors.toml`,
   exactly as before this feature existed.
3. If neither is present, the script exits with a clear error and never touches the
   keyboard.

Both `~/.config/omarchy/themes` and `/usr/share/omarchy/themes` are searched, in that
order, same as before.

```sh
tools/apply-theme.sh                     # current theme, keyboard.rgb or accent
tools/apply-theme.sh 'Tokyo Night'
COLOR_KEY=background tools/apply-theme.sh
```

## The colour profile

`~/.config/dareu-ek75/color-profile.toml` (or `$XDG_CONFIG_HOME/dareu-ek75/...`, or
`$DAREU_COLOR_PROFILE` to point elsewhere). **If this file does not exist, nothing in
this section runs**: the resolved theme colour goes to OpenRGB unmodified, exactly like
before this feature existed. This is the default, and it is intentional — no calibration
has been done for you.

```toml
[keyboard]
brightness = 70          # 0-100, physical brightness. Omitted: brightness is left alone.

[color]
min_lightness = 0.45     # OKLab L clamp, 0-1. Example only, not a real calibration.
max_lightness = 0.85
max_chroma = 0.25        # OKLCH C clamp. Omitted: no chroma limit.

[channels]
red_gain = 1.0
green_gain = 1.0
blue_gain = 1.0

[gamma]
red = 1.0
green = 1.0
blue = 1.0
```

Every field defaults to "do not change the colour": `min_lightness = 0`, `max_lightness = 1`,
no chroma limit, every gain and gamma `1.0`, no brightness override. A profile file with
none of these set, or an empty one, behaves the same as having no file at all (up to
floating-point rounding of a fraction of one 8-bit step).

An `[lut]` section is reserved for a future per-channel `red_lut`/`green_lut`/`blue_lut`
(256 entries each). It is parsed and currently ignored — see [LUT](#lut-reserved-not-implemented).

A malformed file (bad TOML, or a value out of range, e.g. `brightness = 150`) is a hard
error: `apply-theme.sh` exits 1 and never calls `openrgb`, the same way it already does
for a theme that does not exist.

## The transform

`tools/color_transform.py` (a small stdlib-only module, importable and unit-tested —
see `tests/unit/test_color_transform.py`) implements:

```
sRGB -> linear RGB -> OKLab/OKLCh
     -> clamp Lightness to [min_lightness, max_lightness]
     -> clamp Chroma to max_chroma
     -> gamut-map back into sRGB, hue held fixed (binary search on Chroma)
     -> linear RGB -> sRGB
     -> per-channel gain, then per-channel gamma (v ** (1/gamma))
     -> round to 8-bit RGB
```

OKLCh (Björn Ottosson's OKLab, in polar form) is used instead of naive per-channel
scaling because scaling R/G/B directly shifts the perceived hue and lightness together;
adjusting Lightness and Chroma in OKLCh keeps Hue essentially fixed (verified in the
tests to within a fraction of a degree) and clamping happens in a space where "how
saturated" and "how light" are actually separate numbers. Gamut mapping reduces Chroma
along a line of constant Lightness and Hue until the colour is a real sRGB colour again,
rather than clipping each channel independently (which would shift the hue).

This was written by hand instead of adding a colour-management dependency: the whole
pipeline is under 200 lines of stdlib Python (`math` only), it is fully unit-tested, and
pulling in a colour library for one conversion would be a heavier and less transparent
dependency than the couple of well-known matrices this needs.

## Brightness is separate from colour

The OpenRGB CLI takes brightness as its own flag: `-b, --brightness [0-100]`, independent
of `-c`. `tools/apply-theme.sh` only passes `-b` when the colour profile sets
`[keyboard].brightness`; with no profile, brightness is never touched, so opening
OpenRGB or switching themes does not silently change how bright the keyboard is.

## Calibration

`tools/calibrate_color.py` sends predictable sequences to the keyboard so you can see how
it actually responds, through the same OpenRGB build the rest of the project uses
(`$OPENRGB`, defaulting to the installed copy) — it never talks to the HID device
directly.

```sh
tools/calibrate_color.py red                       # 0x10..0xFF ramp, red channel only
tools/calibrate_color.py green
tools/calibrate_color.py blue
tools/calibrate_color.py gray                       # 0x10..0xFF, R=G=B
tools/calibrate_color.py palette                     # red/green/blue/cyan/magenta/yellow/white/gray
tools/calibrate_color.py brightness --color ffffff   # fixed colour, brightness 10%..100%
tools/calibrate_color.py red --interval 5            # slower, for photographing each step
```

Ctrl+C stops after the sample in flight; nothing is left mid-transition. This tool only
*produces* samples — it does not compute gamma, gain or a LUT for you. Turning what you
observe into `color-profile.toml` values is a manual step, and even then it is a visual
approximation, not a colorimetric measurement, unless you have a colorimeter to read the
LEDs with.

## LUT (reserved, not implemented)

The profile format reserves an `[lut]` section for a future per-channel lookup table
(`red_lut`/`green_lut`/`blue_lut`, 256 entries each), to sit right after gain/gamma in
`color_transform.py`, before the final rounding to 8-bit. No values are assumed here —
this is only about leaving the extension point in place. Adding it later needs no change
to `src/DareuEK75Controller/`: the driver still just receives a final RGB.

## Side light

Region 4 (the side light bar) behaves differently from region 1 in a way that predates
this feature and is unrelated to it — see
[`docs/RESEARCH.md`](RESEARCH.md#lighting-hw) and
[`docs/IMPLEMENTATION.md`](IMPLEMENTATION.md#modes):

- `Static` on the side light **ignores the colour** and shows a fixed pattern.
- `Breathing` does take a colour.
- `Off` works.

`tools/apply-theme.sh` only drives region 1 (`Dareu EK75`) today, on purpose — mapping a
second theme colour (e.g. a "secondary" key) onto the side light's `Breathing` mode is a
reasonable future extension, but is not implemented, to keep this first version simple.
Nothing here forces `Static` with a colour onto the side light.

## Why no per-key RGB

Unrelated to theme calibration and already covered in
[`docs/RESEARCH.md`](RESEARCH.md#per-key-colour-openrgb-direct-mode): both regions report
`LedType 4`, the vendor software itself cannot draw per key on this type, and a per-key
frame packet sent through the 2.4G receiver wedges the dongle until it is unplugged. The
driver refuses to send that command
(`DAREU_CLASS_LIGHTING && (command & ~DAREU_CMD_GET) == DAREU_LED_CMD_FRAME`, in
`DareuEK75Controller.cpp`); this feature does not touch that check and never will.
