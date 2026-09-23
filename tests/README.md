# Tests

```sh
make test              # everything, about half a minute
make test-unit
make test-integration
make test-e2e
make test JUNIT=1      # also writes build/tests/*.xml
```

Needs `g++` (or `clang++`), `make`, `python3` (3.11+, for stdlib `tomllib`). `udevadm` is used when
present. No OpenRGB build, Qt or keyboard is needed: the tests run the real driver code against a
software model of the receiver. Written with [Cest](https://github.com/KitsuneSemCalda/Cest),
vendored in `vendor/cest/`.

## Suites

| Suite | Scope | What it checks |
|---|---|---|
| `unit/` (Cest) | `DareuEK75Device`, `DareuEK75Controller` | Packet bytes, reply decoding, slot to target mapping, colour and payload limits, the 20 ms gap, reply polling, the `LED_CMD_FRAME` refusal, thread safety |
| `integration/` (Cest) | detector + controllers + modes | What OpenRGB sees: devices and names, the mode list of each region, speed/colour/brightness flags, reading the keyboard state at load, Direct and Off emulation, brightness and colour throttling |
| `e2e/` (Cest) | the whole system | Full sessions and restarts against the fake, a receiver that stops answering, both regions from two threads; then the real `tools/*.sh`, `install.sh`, the udev rule and `probe.py` run in a scratch `HOME` with a fake `openrgb`, including theme colour resolution (`keyboard.rgb` vs `COLOR_KEY`) and the colour-profile file |
| `test-color-transform` (`unittest`) | `tools/color_transform.py`, `tools/calibrate_color.py` | Hex parsing, the OKLCh transform (extremes, lightness/chroma clamping, hue preservation, gain, gamma), profile loading and validation, calibration sequence generation, Ctrl+C handling — no OpenRGB or shell involved |

The e2e suite cannot cover the real hardware or the real OpenRGB core: the receiver is the fake,
and `install-udev.sh` needs root so only the rule file itself is checked (`udevadm verify`).

## The fake receiver

`fake/fake_receiver.*` implements the hidapi calls the driver makes, following
[`docs/RESEARCH.md`](../docs/RESEARCH.md):

- target `0` reaches the receiver, `(slot + 1) << 4` the keyboard; anything else gets no reply
- replies are asynchronous (`not_ready_polls` polls before the answer shows up)
- commands closer than 15 ms are dropped
- one `LED_CMD_FRAME` wedges it for good

Tests configure it through `g_receiver` and read back what the driver sent and what state the
"firmware" ended up in. Its behaviour is only as good as the research notes; when the hardware
disagrees, fix `RESEARCH.md` and the fake together.

## Stubs

`stubs/` replaces `RGBController.h`, `DetectionManager.h`, `LogManager.h` and `hidapi.h` with
the minimum the driver uses. They can drift from OpenRGB; the `build` job in CI compiles the
driver into the real OpenRGB for that reason.

## Writing tests

Cest's `describe`/`it` are macros, so a comma outside parentheses inside a block splits its
arguments. Wrap braced lists with `Cols({...})` / `Bytes({...})` from `common/helpers.h`,
and use `I(x)` to compare unsigned values.

A test that passes proves little until it has failed once. When adding one, break the code it
covers and check that it goes red.
