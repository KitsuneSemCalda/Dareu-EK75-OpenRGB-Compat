#!/usr/bin/env bash
# Full install: udev rule (asks for sudo), OpenRGB build with the driver, Omarchy theme hook.
set -euo pipefail

cd "$(dirname "$0")"
tools/install-udev.sh
tools/build.sh
tools/install-launcher.sh
tools/install-hook.sh
