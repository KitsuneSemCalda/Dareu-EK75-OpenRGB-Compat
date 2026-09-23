# Implementation notes

How the driver in `src/DareuEK75Controller/` is put together and why. The wire protocol and what
was verified on hardware are in [RESEARCH.md](RESEARCH.md); this file is about the code.

## Layers

```
OpenRGB core
   |  DetectDareuEK75()                 DareuEK75ControllerDetect.cpp
   |    one call per matching HID interface: 260d:0042 (receiver) or
   |    260d:0045 (direct USB), both interface 3
   v
RGBController_DareuEK75  (x2)           RGBController_DareuEK75.cpp
   |  one per region: "Dareu EK75" and "Dareu EK75 Side Light"
   |  modes, zones, what the UI shows
   v
DareuEK75Controller      (x2)           DareuEK75Controller.cpp
   |  thin facade that fixes the region number
   v
DareuEK75Device          (x1, shared)   DareuEK75Controller.cpp
      the HID handle, the target id, Transfer()
```

The device is shared because there is a single control channel (the receiver, or the keyboard
itself when wired). Both regions go through one `Transfer()`, which holds a mutex, so two OpenRGB
threads cannot interleave their packets. The handle closes when the last region controller drops
its `shared_ptr`.

## Detection

`REGISTER_HID_DETECTOR_I` matches vendor id, product id and **interface number**; it is called
twice, once for `DAREU_EK75_RECEIVER_PID` (0x0042) and once for `DAREU_EK75_WIRED_PID` (0x0045),
both on `DAREU_EK75_VENDOR_INTERFACE` (3) — the only one of the five HID interfaces that carries
the vendor feature report (usage page `0xFF00`). Matching on the interface keeps the driver from
opening the other four.

`Connect()` branches on which PID was detected (stored as `pid`, distinct from `keyboard_pid`,
which is what the *keyboard itself* reports and is only known after connecting):

- Receiver PID: ask the receiver, not the keyboard, which slots are paired, and remember the
  first paired one as `target_id = (slot + 1) << 4`. If no slot is paired (keyboard off, or its
  switch is not on 2.4G) detection returns nothing and logs a hint.
- Wired PID: no handshake, no slot. `target_id = 0` reaches the keyboard directly — verified [hw],
  see [`docs/RESEARCH.md`](RESEARCH.md#direct-usb-wired). `Connect()` always succeeds for this PID;
  a keyboard that somehow does not answer just fails the region query right after, same as
  any other unreachable device.

Each region is then queried with `LED_CMD_ATTRIBUTE`. A region the firmware does not report
produces no device. This step, and everything below it, is identical on both transports.

## One transfer

`Transfer()` in `DareuEK75Controller.cpp` is the only code that touches the device:

1. Refuse `LED_CMD_FRAME` (see below).
2. Build the 65 byte buffer (byte 0 is the unused report ID, the packet follows).
3. Wait until 20 ms after the end of the previous transfer.
4. `hid_send_feature_report`.
5. Poll `hid_get_feature_report` every 10 ms, up to 20 times, until the low nibble of the first
   byte is 2 and the class byte matches.
6. Copy the reply out without the report ID, so `reply[6]` is the first payload byte on both
   sides of the wire.

The delays are not cosmetic: commands sent too close together are dropped by the receiver, and a
missing reply makes `Transfer()` return false and logs at debug level.

## Modes

Modes are generated from data, not written one by one. At load the controller reads the region's
effect list from the firmware, and `dareu_effects[]` in `RGBController_DareuEK75.cpp` supplies the
name, whether the effect takes a speed and how many colours it takes. An effect id the table does
not know is skipped, so a firmware with extra effects does not break anything, it just hides them.

Three modes are not firmware effects:

| Mode | Value | What it sends |
|---|---|---|
| Direct | `0x100` | Static with the LED's colour |
| Off (keys) | `0x101` | Static with black |
| Off (side light) | `0` | the firmware's own Off, region 4 has one |

Values above `0xFF` cannot collide with an effect id, which is a single byte. Direct is there so
that OpenRGB clients that only know Direct still work; it colours the whole region, not each key.

Other rules worth knowing before changing the table:

- Brightness is a separate command. `ApplyMode` sends the effect first, then the brightness only if
  it differs from `last_brightness`. Off has no brightness.
- An empty colour list is meaningful. Animated effects then cycle the rainbow, which is how
  "random colour" is expressed. Static needs a colour, so it never gets that flag.
- Region 4 Static ignores the colour and draws a fixed pattern, so that mode is exposed with no
  colour picker.
- Speed is 1 to 3 and `flag` (direction) is stored but never rendered, so it is not exposed.

## Reading state back

`LoadCurrentState()` reads the current effect and brightness from the keyboard when the device is
created, and selects the matching mode. Starting OpenRGB therefore does not change the lighting.
Static is read back as Static, because Direct and Off-as-black are also Static on the wire and
cannot be told apart.

## Why there is no per-key colour

Two independent reasons, both in [RESEARCH.md](RESEARCH.md#per-key-colour-openrgb-direct-mode):

- The firmware reports both regions as `LedType 4`, and the vendor software only implements frame
  drawing for other types. Even on the wired keyboard, open-ek75 saw sixteen LEDs sent at index 0 light the whole board.
- Through the receiver, a frame packet wedges it until it is unplugged.

Because of the second one, `Transfer()` contains a hard refusal of `LED_CMD_FRAME`. It is a
safety net and should stay even if per-key is ever attempted on other hardware; anyone
experimenting has to remove it deliberately, on their own risk, and expect to re-plug the receiver.

## Scripts

| Script | Notes |
|---|---|
| `tools/build.sh` | Clones OpenRGB into `build/`, copies `src/DareuEK75Controller` into `Controllers/` (which `OpenRGB.pro` globs, so no project file edits), builds with `qmake6`. Without `lrelease` it strips the embedded translations from `OpenRGB.pro` instead of failing. |
| `tools/install-udev.sh` | Reruns itself under `sudo`, removes the old `99-` rule, installs the new one and retriggers hidraw and usb so it applies without replugging. |
| `tools/install-launcher.sh` | Copies the system `.desktop` entry to `~/.local/share/applications` with the same file name, so it overrides the menu launcher, and patches `Exec=` in the autostart entry if there is one. |
| `tools/apply-theme.sh` | Resolves a colour from an Omarchy theme and runs `openrgb -m Static -c` (`--noautoconnect`, so it does not try to reach a running OpenRGB server). Colour resolution, the optional perceptual transform and brightness are theme policy, not driver behaviour — see [docs/THEME_COLOR.md](THEME_COLOR.md). |
| `tools/install-hook.sh` | Installs `apply-theme.sh` as a `theme-set.d` hook (a plain copy, not a symlink), plus the `color_transform.py` helper it depends on. |
| `tools/probe.py` | Standalone protocol probe with no OpenRGB. Finds the hidraw node by vendor id plus the descriptor bytes `06 00 ff` and `95 40 b1`, so it does not depend on the interface number. |
