# Contributing

## Build and test

```sh
make test                 # no OpenRGB or hardware needed, see tests/README.md
tools/build.sh            # builds OpenRGB with the driver, needs the Qt 6 build dependencies
```

Run `make test` before opening a pull request. CI also runs `shellcheck`, the tests under
ASan/UBSan and clang, and compiles the driver into the real OpenRGB.

## Commits

Small commits, one logical change each, [Conventional Commits](https://www.conventionalcommits.org/)
(`feat:`, `fix:`, `docs:`, `test:`, `build:`, `ci:`, `refactor:`, `chore:`), written in English.

## Licenses

`src/DareuEK75Controller/` is GPL-2.0-or-later because it is built into OpenRGB. Everything else is
MIT. See the README.

## Hardware notes

Anything that is a claim about the keyboard goes in `docs/RESEARCH.md` marked as verified on
hardware or taken from a reference. **Never send `LED_CMD_FRAME` through the receiver**: it wedges
the receiver until it is unplugged, and the driver refuses to send it on purpose.

To report a keyboard that behaves differently, run `tools/probe.py` and include its output, your
`lsusb -d 260d:` line and the OpenRGB log.
