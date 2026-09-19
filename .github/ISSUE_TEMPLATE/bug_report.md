---
name: Bug report
about: The keyboard is not detected or the lighting does not do what OpenRGB shows
labels: bug
---

**What happens**

**What you expected**

**Setup**
- Keyboard mode switch position (must be 2.4G):
- `lsusb -d 260d:`:
- Distribution and OpenRGB version (`build/OpenRGB/openrgb --version`):
- Output of `build/OpenRGB/openrgb --list-devices`:
- `ls -l /dev/hidraw*` for the receiver, and whether `tools/install-udev.sh` was run:

**Log**
Run OpenRGB with `--loglevel 5` and paste the lines that start with `[Dareu EK75]`.
