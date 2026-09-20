# Dareu EK75 + OpenRGB

[![CI](https://github.com/KitsuneSemCalda/Dareu-EK75-OpenRGB-Compat/actions/workflows/ci.yml/badge.svg)](https://github.com/KitsuneSemCalda/Dareu-EK75-OpenRGB-Compat/actions/workflows/ci.yml)
[![License: MIT + GPL-2.0](https://img.shields.io/badge/license-MIT%20%2B%20GPL--2.0-blue.svg)](#license)

An [OpenRGB](https://openrgb.org) controller for the Dareu EK75 (TK51G) keyboard on Linux, working
through its 2.4G receiver (`260d:0042`). When it was written, no OpenRGB support or issue for Dareu
keyboards could be found. The repository holds the controller, the scripts that build and install
it, the protocol notes, and a test suite that runs without the keyboard.

```console
$ build/OpenRGB/openrgb -ld
0: Dareu EK75
  Type:           Keyboard
  Location:       HID: /dev/hidraw5 region 1
  Modes: Direct [Static] Breathing Neon Reactive Wave Raindrop 'Running Light' Rotate Starlit
         'Rainbow Wave' 'Light Wave' 'Steady Stream' 'Area Reactive' 'Line Reactive' Waterfall
         Scanning Heartbeat Fluxay 'Heart Breath' 'Moon Breath' 'Star Breath' Off

1: Dareu EK75 Side Light
  Type:           LED Strip
  Location:       HID: /dev/hidraw5 region 4
  Modes: Off [Static] Breathing
```

## What works

| Device | Modes | Controls |
|---|---|---|
| **Dareu EK75** (key backlight, region 1) | Every effect the firmware reports, plus `Direct` and `Off` | Colour (up to 5 for Breathing), speed 1 to 3, brightness |
| **Dareu EK75 Side Light** (light bar, region 4) | `Off`, `Static`, `Breathing` | Colour for Breathing, brightness |

The current state of the keyboard is read when OpenRGB starts, so opening it does not change the
lighting.

### Limits

- **No per-key colour.** The firmware reports both regions as `LedType 4`, which not even Dareu's
  own software can draw per key, and a frame packet sent through the receiver hangs it until it is
  unplugged. The driver refuses to send one. `Direct` therefore paints the whole region with one
  colour. The evidence is in [docs/RESEARCH.md](docs/RESEARCH.md).
- **2.4G receiver only.** The keyboard's mode switch must be on 2.4G. Wired mode (`260d:0101`) is not
  registered because its HID interface layout has not been verified.
- **Side light Static** ignores the colour and shows a fixed pattern, so that mode has no colour picker.

## Install

```sh
./install.sh
```

It needs the Qt 6 build dependencies of OpenRGB and runs four steps:

1. `tools/install-udev.sh` lets your user open the receiver's hidraw node (asks for sudo)
2. `tools/build.sh` clones OpenRGB into `build/`, adds `src/DareuEK75Controller` and compiles
3. `tools/install-launcher.sh` copies the build to `~/.local/lib/dareu-ek75/openrgb`
   and points the OpenRGB menu entry and login autostart at that copy,
   because the packaged `openrgb` has no EK75 driver
4. `tools/install-hook.sh` colours the keyboard with the accent of every Omarchy theme you switch to

The launcher and theme hook use installed copies, so moving or deleting this
checkout does not break them. After rebuilding, run `tools/install-launcher.sh`
again to update the installed binary. Run `tools/install-hook.sh` again after
changing the theme script. Direct use of `tools/apply-theme.sh` defaults to the
installed binary; set `OPENRGB=/path/to/openrgb` to test another build.

Then check it:

```sh
build/OpenRGB/openrgb --list-devices
```

## Usage

Use the OpenRGB window as usual, or the command line:

```sh
build/OpenRGB/openrgb -d "Dareu EK75" -m Static -c 7aa2f7
build/OpenRGB/openrgb -ld                     # devices and their modes
```

`tools/apply-theme.sh [theme]` sets the key backlight to the accent colour of an Omarchy theme, by
default the current one. `COLOR_KEY=background tools/apply-theme.sh` picks another colour from the
theme's `colors.toml`. The keyboard takes one colour per region, so a whole palette cannot be shown.

## Troubleshooting

| Symptom | Check |
|---|---|
| No `Dareu EK75` in `--list-devices` | The mode switch must be on 2.4G and the keyboard awake. Run with `--loglevel 5 -v`: `No keyboard connected to the receiver` means the receiver sees no paired keyboard. |
| Nothing at all, not even in the log | The user cannot open the hidraw node. Run `tools/install-udev.sh`, which also re-applies the rule to the connected receiver. |
| The packaged `openrgb` does not list it | Only the build made by `tools/build.sh` has the driver. `tools/install-launcher.sh` points the menu entry at it. |
| The receiver stops responding | Replug it. |

## How it works

The receiver has one vendor HID interface (interface 3, usage page `0xFF00`). Every command is a
64 byte feature report, and the keyboard behind the receiver is addressed as `(slot + 1) << 4`.
The receiver answers asynchronously and drops commands that come too close together, so the driver
polls for the reply and keeps a gap between transfers. Modes are built from the effect list the
firmware reports instead of being hard coded per model.

- [docs/IMPLEMENTATION.md](docs/IMPLEMENTATION.md): layers, one transfer step by step, how modes are built
- [docs/RESEARCH.md](docs/RESEARCH.md): the protocol, what was verified on this keyboard and what was taken from a reference

## Repository

| Path | What |
|---|---|
| `src/DareuEK75Controller/` | The OpenRGB controller, copied into `Controllers/` by `tools/build.sh` |
| `tools/` | Build, install and theme scripts, and `probe.py`, a stdlib probe for the protocol without OpenRGB |
| `udev/70-dareu-ek75.rules` | `uaccess` rule (the file name prefix must be below 73) |
| `tests/` | Unit, integration and e2e tests against a software model of the receiver ([tests/README.md](tests/README.md)) |
| `docs/` | Research notes and implementation notes |

## Development

```sh
make test    # about half a minute, no OpenRGB build or keyboard needed
```

CI runs the tests on gcc and clang and under ASan and UBSan, lints the scripts, and compiles the
driver into the real OpenRGB. See [CONTRIBUTING.md](CONTRIBUTING.md). Another Dareu model or the
wired keyboard? [Open a hardware report](../../issues/new?template=hardware_report.md) with the
output of `tools/probe.py`.

## Credits

The protocol was reverse engineered by [open-ek75](https://github.com/mateusands/open-ek75) (GPL-3.0).
`docs/RESEARCH.md` records which facts come from it and which were verified on this keyboard.
Tests use [Cest](https://github.com/KitsuneSemCalda/Cest).

## License

Two licenses apply, by directory:

- `src/DareuEK75Controller/` is **GPL-2.0-or-later** ([LICENSE](src/DareuEK75Controller/LICENSE)).
  It is compiled into OpenRGB and uses its API, so it carries OpenRGB's license.
- Everything else (scripts, tests, docs, udev rule) is **MIT** ([LICENSE](LICENSE)).

`tests/vendor/cest/` keeps Cest's own BSD-3-Clause license.
