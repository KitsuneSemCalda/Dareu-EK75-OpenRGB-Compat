# Research: Dareu EK75 + OpenRGB

Findings that drive the design. Everything marked **[hw]** was verified on this
machine's keyboard, either through its 2.4G dongle or over a direct USB cable (both are
noted where it matters); everything marked **[ref]** comes from the open-ek75 project
(https://github.com/mateusands/open-ek75, GPL-3.0, recovered from Dareu's web driver and
Husky's Windows app) and was tested there on the wired keyboard only.

## Hardware

| Item | Value |
|---|---|
| Keyboard | Dareu EK75, model `TK51G`, profile `https://dr.dareu.com/products/0045/0045.json` **[ref]** |
| Dongle (USB) | `260d:0042` "USB 2.4G Receiver", 5 HID interfaces **[hw]** |
| Keyboard PID via dongle | `0x0045` **[hw]** |
| Keyboard PID over direct USB | `260d:0045` **[hw]**, product string `EK75_Keyboard`, 5 HID interfaces — see [Direct USB (wired)](#direct-usb-wired). Corrects an earlier guess of `260d:0101`, taken from open-ek75 **[ref]** and never verified on this unit; that PID is kept in the udev rule in case another EK75 revision uses it. |
| Control interface | interface 3, usage page `0xFF00`, 64-byte feature report, no Report ID **[hw]**, same on both transports |
| Detection signature | descriptor contains `06 00 ff` and `95 40 b1` |

## Transport **[hw]**

- Linux: `HIDIOCSFEATURE` then `HIDIOCGFEATURE`, 65-byte buffers (byte 0 = 0).
- The device replies asynchronously. Poll `GET_FEATURE` until `(reply[0] & 0x0F) == 2`
  (up to 20 tries, 10 ms apart).
- Packet: `[TargetId, Size, Class, Cmd, Profile, 0, payload...]`, `Cmd` has `0x80` set for GET.
- **`TargetId` through the dongle is `(slot + 1) << 4`, so `0x10` for slot 0.**
  open-ek75 says configuration does not work through the dongle; that is wrong for
  this unit: reads and writes both work with `TargetId 0x10`. `TargetId 0` reaches
  only the dongle itself (e.g. wireless status).
- Wireless status (`class 0, cmd 0x20|GET, size 7, profile 0`): reply byte 6 = slot
  count, then `status, pid_hi, pid_lo` per slot. Ours: `01 | 01 00 45`.

## Direct USB (wired) **[hw]**

Verified on this unit by plugging the keyboard in directly instead of through the dongle,
using `tools/probe.py`-style GET-only queries (see [Per-key colour](#per-key-colour-openrgb-direct-mode)
below for why nothing beyond GET was tried here).

- Enumerates as `260d:0045`, product string `EK75_Keyboard`, 5 HID interfaces, same
  layout as through the dongle. The vendor interface (usage page `0xFF00`, descriptor
  signature `06 00 ff` / `95 40 b1`) is on interface 3, same as the dongle.
- The class 0 wireless-status query (`class 0, cmd 0x20|GET`) gets **no reply** at all,
  any target. Expected: there is no dongle to interrogate, and this class is receiver-only.
- Class 3 lighting queries get a normal reply at **`TargetId 0`**, and also at `TargetId 0x10`
  (the dongle's slot-0 address) — the keyboard does not appear to check the target at all
  when there is no dongle multiplexing several slots. Captured on this unit, region 1:
  - `ATTRIBUTE get`: `region=1, type=4, fps=0x21(33), rows=6, columns=0x0f(15)`, effect list
    `01 02 05 0b 04 09 06 03 0a 14 15 16 ...` — byte-for-byte the same region-1 attributes and
    effect table as through the dongle (see [Lighting](#lighting-hw) below).
  - `EFFECT get`: `region=1, effect=6 (Raindrop), flag=0, speed=1, ncolors=1, rgb=ff ff ff`
    (whatever the keyboard happened to be showing at probe time).
  - `BRIGHTNESS get`: `region=1, level=0xff`.
- Conclusion: identical protocol, packet layout and effect table on both transports. The
  driver picks the addressing scheme from the PID it was detected on
  (`DareuEK75Device::Connect()`): the receiver's pairing handshake for `0x0042`, straight to
  `TargetId 0` for `0x0045`, no handshake. `LED_CMD_FRAME` was **not** tested over this
  transport and the driver's refusal to ever send it (see below) is unconditional either way.

## Lighting **[hw]**

`class 3`. Region 1 = key matrix (6x15, type 4, 33 fps). Region 4 = side light bar
(1x16, type 4). Region list reply: `02 00 01 04` = regions 1 and 4.

| Cmd | Payload | Notes |
|---|---|---|
| `2` EFFECT get | `region` | reply: `region, effect, flag, speed, ncolors, rgb*n` |
| `2` EFFECT set | `region, effect, flag, speed, ncolors, rgb*n`; size `5+3n`, profile 1 | echo confirms |
| `3` BRIGHTNESS | `region[, level]`; get size 1..2, set size 2 | 0-255 |
| `1` ATTRIBUTE get | `region`, profile 0 | type, fps, matrix, effect list |

Effects the firmware reports **[hw]**:

- Region 1: `1 Static, 2 Breathing, 5 Wave, 11 Starlit, 4 Reactive, 9 RunningLight,
  6 Raindrop, 3 Neon, 10 Rotate, 20 RainbowW, 21 LightWave, 22 SteadyStream,
  24 AreaReactive, 25 LineReactive, 26 Waterfall, 27 Scanning, 28 Heartbeat,
  29 Fluxay, 30 HeartBreath, 31 MoonBreath, 32 StarBreath, 18 StreamingFrame`.
  Not listed: 0 Off (open-ek75 found that Off only works on region 4).
- Region 4: `0 Off, 2 Breathing, 1 Static, 18 StreamingFrame`. Static ignores the colour
  and shows a fixed 16-colour pattern **[ref]**.

Fields: speed 1-3, flag = direction (stored but not rendered), up to 5 colours **[ref]**.
Colour count per effect follows `CheckCustomColorCount`; when unsure, send 1 colour.

## Per-key colour (OpenRGB "Direct" mode)

**Not available on this hardware, and dangerous to probe.**

- `LED_CMD_FRAME (4)` layout is known: payload `region, flags(0x00 / 0x80 last), frame#,
  first, last, rgb*n`, `size = 6 + 3n`, profile not written, 16 LEDs per packet **[ref]**.
- Wired, open-ek75 sent frames at 120 pkt/s: accepted, but sixteen red LEDs at index 0 lit
  the whole keyboard blue, and `first/last` were ignored. Both regions are `LedType 4`; the
  vendor software only implements frames for types 1 and 3, so nobody has ever driven per-key
  on this model **[ref]**.
- Through the dongle, a frame packet **wedges the dongle**: `EPROTO` / `ETIMEDOUT`, the USB
  device drops and has to be re-plugged. Reproduced three times with different flags/profile
  bytes **[hw]**. **Do not send `LED_CMD_FRAME` through the dongle.**
- Consequence: `Direct` cannot be per-key. The OpenRGB controller exposes whole-region colour
  through Static/Breathing instead.

## LED layout (`LedMatrix`, identical in profiles 0045 and 0101)

Six rows of fifteen, KeyIDs, 0 = no LED:

```
Esc F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11 F12  .   .
 ~  1  2  3  4  5  6  7  8  9  0   -   =  Bksp PgUp
Tab Q  W  E  R  T  Y  U  I  O  P   [   ]   \   Del
Caps A S  D  F  G  H  J  K  L  ;   '   .  Ent PgDn
LSh Z  X  C  V  B  N  M  ,  .  /  RSh Up   .   .
LCt LWin LAlt . . Space . . . RAlt Fn RCt Lft Dn Rgt
```

Only relevant if per-key ever works; keep for the OpenRGB matrix map.

## Other facts

- Battery: `class 7, cmd 0|GET, size 3` -> `status, level, max, critical` **[ref]**.
- Fn brightness ladder: 0 -> 70 -> 120 -> 190 -> 255 **[ref]**.
- No existing OpenRGB support or issue for Dareu keyboards was found.
- `dr.dareu.com` publishes a JSON profile per PID: `https://dr.dareu.com/products/<PID>/<PID>.json`.

## Design decisions for the OpenRGB controller

1. Detect on `260d:0042` (2.4G receiver) and `260d:0045` (direct USB), both interface 3
   (usage page `0xFF00`). `260d:0101`, the wired PID guessed from the open-ek75 reference,
   is not registered: it was never seen on this unit and would need its own verification.
2. Through the receiver, probe wireless status at load and pick `TargetId = (slot+1)<<4`
   for the first slot that reports a keyboard; without a paired keyboard nothing is
   detected. Over direct USB, skip that handshake entirely and use `TargetId 0`, which the
   keyboard answers directly (see [Direct USB (wired)](#direct-usb-wired)).
3. Two OpenRGB devices, `Dareu EK75` (region 1) and `Dareu EK75 Side Light` (region 4), each with
   one zone and one LED, since the firmware takes one colour per region.
4. Modes come from the firmware's own effect list (query `LED_CMD_ATTRIBUTE` at load). The
   mode active at start is the one the keyboard is already in. No frame streaming, ever.
5. Rate-limit writes: at least 20 ms between commands, counted from the end of the previous
   reply (the firmware echoes asynchronously).
