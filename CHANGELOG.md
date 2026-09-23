# Changelog

## 0.2.0 - 2026-09-22

### Added
- Direct USB support (`260d:0045`), verified `[hw]`: same vendor interface, protocol and effect
  table as the 2.4G receiver. `DareuEK75Device::Connect()` skips the receiver's pairing handshake
  and addresses the keyboard at `TargetId 0` directly for this PID. See
  `docs/RESEARCH.md#direct-usb-wired`.
- `tools/apply-theme.sh` now resolves the theme colour from a `keyboard.rgb` file in the theme
  directory first, falling back to `COLOR_KEY` (default `accent`) from `colors.toml`.
- An optional perceptual colour transform for the Omarchy integration (`tools/color_transform.py`,
  `~/.config/dareu-ek75/color-profile.toml`): OKLCh lightness/chroma clamping, per-channel gain and
  gamma, and a separate physical brightness, applied before the (unchanged) driver. No profile file:
  behaviour is identical to before this existed. See `docs/THEME_COLOR.md`.
- `tools/calibrate_color.py`, sending predictable colour/brightness sequences for studying the
  keyboard's real response.

### Changed
- Corrected the wired PID in `docs/RESEARCH.md`: `260d:0101` (from the open-ek75 reference) was
  never verified on this unit and does not match what it actually reports (`260d:0045`). Both PIDs
  are still in the udev rule; only `0x0045` is registered by the driver.
- `tools/apply-theme.sh` no longer forces `Static`. It omits `-m`, so OpenRGB reapplies the colour
  to whichever mode the keyboard is already in (Raindrop, Breathing, ...); a mode with no colour
  slot is left animating, untouched.

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
