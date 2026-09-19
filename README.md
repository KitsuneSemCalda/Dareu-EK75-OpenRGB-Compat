# Dareu EK75 + OpenRGB

OpenRGB controller for the Dareu EK75 (TK51G) keyboard, driven through its 2.4G receiver
(`260d:0042`).

The keyboard shows up in OpenRGB as two devices:

- **Dareu EK75**: the key backlight (region 1), all of the firmware's effects, speed, colours and
  brightness, plus `Direct` and `Off` emulated with the Static effect.
- **Dareu EK75 Side Light**: the side light bar (region 4), which supports Off, Static and Breathing.

**Not supported: per-key colour.** The firmware exposes both regions as `LedType 4`, which not even
Dareu's own software can drive per key, and sending frame commands hangs the receiver. See
[docs/RESEARCH.md](docs/RESEARCH.md) for the evidence.

## Install

```sh
./install.sh
```

That runs, in order:

1. `tools/install-udev.sh`: lets your user open the receiver's hidraw node (asks for sudo)
2. `tools/build.sh`: clones OpenRGB into `build/`, adds `src/DareuEK75Controller`, compiles
3. `tools/install-launcher.sh`: points the OpenRGB menu entry and login autostart at this build (the system `openrgb` has no EK75 driver)
4. `tools/install-hook.sh`: makes every Omarchy theme change colour the keyboard with the theme's accent

Then `build/OpenRGB/openrgb --list-devices`. The keyboard's mode switch has to be on 2.4G.
`tools/apply-theme.sh [theme]` applies a theme by hand; `COLOR_KEY=magenta` picks another colour.

## Layout

| Path | What |
|---|---|
| `src/DareuEK75Controller/` | The OpenRGB controller (copied into `Controllers/` by `tools/build.sh`) |
| `tools/probe.py` | Stdlib Python probe for poking the protocol without OpenRGB |
| `udev/70-dareu-ek75.rules` | `uaccess` rule (the prefix must be below 73) |
| `docs/RESEARCH.md` | Protocol notes, what was verified and what is known not to work |

## Status

- Wired mode (`260d:0101`) is not registered: its HID interface layout is unverified.
- Protocol credit: [open-ek75](https://github.com/mateusands/open-ek75).
