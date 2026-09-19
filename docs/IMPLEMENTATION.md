# Implementation notes

How the driver in `src/DareuEK75Controller/` is put together and why. The wire protocol and what
was verified on hardware are in [RESEARCH.md](RESEARCH.md); this file is about the code.

## Layers

```
OpenRGB core
   |  DetectDareuEK75()                 DareuEK75ControllerDetect.cpp
   |    one call per matching HID interface (260d:0042, interface 3)
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

The device is shared because the receiver has a single control channel. Both regions go through
one `Transfer()`, which holds a mutex, so two OpenRGB threads cannot interleave their packets. The
handle closes when the last region controller drops its `shared_ptr`.

## Detection

`REGISTER_HID_DETECTOR_I` matches vendor id, product id and **interface number**. The receiver
exposes five HID interfaces and only interface 3 carries the vendor feature report
(usage page `0xFF00`). Matching on the interface keeps the driver from opening the other four.

`Connect()` then asks the receiver, not the keyboard, which slots are paired and remembers the
first one as `target_id = (slot + 1) << 4`. If no slot is paired (keyboard off, or its switch is
not on 2.4G) detection returns nothing and logs a hint.

Each region is then queried with `LED_CMD_ATTRIBUTE`. A region the firmware does not report
produces no device.

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
| `tools/apply-theme.sh` | Reads one colour key from an Omarchy theme's `colors.toml` and runs `openrgb -m Static -c`. Uses `--noautoconnect` so it does not try to reach a running OpenRGB server. |
| `tools/install-hook.sh` | Writes a `theme-set.d` hook that `exec`s `apply-theme.sh` with an absolute path. |
| `tools/probe.py` | Standalone protocol probe with no OpenRGB. Finds the hidraw node by vendor id plus the descriptor bytes `06 00 ff` and `95 40 b1`, so it does not depend on the interface number. |
