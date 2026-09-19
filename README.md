# Dareu EK75 + OpenRGB

[![CI](https://github.com/KitsuneSemCalda/Dareu-EK75-OpenRGB-Compat/actions/workflows/ci.yml/badge.svg)](https://github.com/KitsuneSemCalda/Dareu-EK75-OpenRGB-Compat/actions/workflows/ci.yml)
[![License: MIT + GPL-2.0](https://img.shields.io/badge/license-MIT%20%2B%20GPL--2.0-blue.svg)](#license)

Control the lighting of the Dareu EK75 (TK51G) keyboard from [OpenRGB](https://openrgb.org) on Linux,
through its 2.4G receiver (`260d:0042`). When this was written no OpenRGB support or issue for Dareu keyboards
was found; this is a controller for it, plus the scripts to build, install and drive it.

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

## Troubleshooting

| Symptom | Check |
|---|---|
| No `Dareu EK75` in `--list-devices` | The keyboard's mode switch must be on 2.4G and the keyboard awake. Run with `--loglevel 5 -v`: `No keyboard connected to the receiver` means the receiver sees no paired keyboard. |
| Nothing at all, not even in the log | The user cannot open the hidraw node: run `tools/install-udev.sh`, which also re-applies the rule to the connected receiver. |
| `openrgb` from the package does not list it | Only the build made by `tools/build.sh` has the driver. `tools/install-launcher.sh` points the menu entry at it. |
| The receiver stops responding | Replug it. The driver never sends the per-key frame command that is known to hang it. |

## Layout

| Path | What |
|---|---|
| `src/DareuEK75Controller/` | The OpenRGB controller (copied into `Controllers/` by `tools/build.sh`) |
| `tools/probe.py` | Stdlib Python probe for poking the protocol without OpenRGB |
| `udev/70-dareu-ek75.rules` | `uaccess` rule (the prefix must be below 73) |
| `tests/` | Unit, integration and e2e tests against a software model of the receiver, see [tests/README.md](tests/README.md) |
| `docs/RESEARCH.md` | Protocol notes, what was verified and what is known not to work |
| `docs/IMPLEMENTATION.md` | How the driver is structured and why: layers, transfers, modes, scripts |

## Development

```sh
make test    # about half a minute, no OpenRGB build or hardware needed
```

See [CONTRIBUTING.md](CONTRIBUTING.md). CI runs the tests on gcc and clang, under ASan/UBSan, and builds
the driver into the real OpenRGB.

## Status

- Wired mode (`260d:0101`) is not registered: its HID interface layout is unverified.
- Protocol credit: [open-ek75](https://github.com/mateusands/open-ek75).

## License

Two licenses apply, by directory:

- `src/DareuEK75Controller/` is **GPL-2.0-or-later** ([LICENSE](src/DareuEK75Controller/LICENSE)).
  It is compiled into OpenRGB and uses its API, so it has to carry OpenRGB's license.
- Everything else (scripts, tests, docs, udev rule) is **MIT** ([LICENSE](LICENSE)).

`tests/vendor/cest/` is [Cest](https://github.com/KitsuneSemCalda/Cest) under its own BSD-3-Clause license.
The protocol was reverse engineered by [open-ek75](https://github.com/mateusands/open-ek75) (GPL-3.0);
`docs/RESEARCH.md` records which protocol facts come from it and which were verified on hardware.
