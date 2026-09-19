# Changelog

## 0.1.0 - 2026-09-19

First release.

### Added
- OpenRGB controller for the Dareu EK75 (TK51G) through its 2.4G receiver (`260d:0042`), as two
  devices: the key backlight (region 1) and the side light bar (region 4).
- Every effect the firmware reports, with colour, speed and brightness; `Direct` and `Off` on the
  keys, emulated with the Static effect.
- The current keyboard state is read at start, so opening OpenRGB does not change the lighting.
- `install.sh` with udev rule, OpenRGB build with the driver, launcher entries and an Omarchy
  theme hook that colours the keyboard with the theme accent.
- `tools/probe.py`, a stdlib probe for the protocol without OpenRGB.
- Protocol notes (`docs/RESEARCH.md`) and implementation notes (`docs/IMPLEMENTATION.md`).
- Unit, integration and e2e tests against a software model of the receiver, and CI on gcc, clang,
  ASan/UBSan and a build against the real OpenRGB.

### Fixed
- `memcpy` from a null pointer when a request has no payload (found by UBSan).

### Not supported
- Per-key colour: the firmware does not support it on this model, and a frame packet hangs the
  receiver, so the driver refuses to send one.
- Wired mode (`260d:0101`): its HID interface layout is unverified.

### Licenses
- `src/DareuEK75Controller/` is GPL-2.0-or-later, everything else MIT.
