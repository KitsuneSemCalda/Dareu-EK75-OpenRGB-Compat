#!/usr/bin/env bash
# Build OpenRGB with the Dareu EK75 controller from src/ compiled in.
# Usage: tools/build.sh [--update] [--sync-only]
#   --update     fetch the latest OpenRGB before building
#   --sync-only  only copy src/ into the OpenRGB tree
# OPENRGB_DIR overrides where the OpenRGB tree lives (default build/OpenRGB).
set -euo pipefail

cd "$(dirname "$0")/.."
OPENRGB_DIR=${OPENRGB_DIR:-build/OpenRGB}
OPENRGB_URL=https://gitlab.com/CalcProgrammer1/OpenRGB.git

if [[ ! -d $OPENRGB_DIR ]]; then
    git clone --depth 1 "$OPENRGB_URL" "$OPENRGB_DIR"
fi

if [[ ${1:-} == --update ]]; then
    shift
    git -C "$OPENRGB_DIR" fetch --depth 1 origin
    git -C "$OPENRGB_DIR" reset --hard FETCH_HEAD
fi

# Controllers/ is globbed by OpenRGB.pro, so copying the directory is enough.
rm -rf "$OPENRGB_DIR/Controllers/DareuEK75Controller"
cp -r src/DareuEK75Controller "$OPENRGB_DIR/Controllers/DareuEK75Controller"

[[ ${1:-} == --sync-only ]] && exit 0

cd "$OPENRGB_DIR"

# Embedded translations need lrelease (qt6-tools). Build without them when it is missing.
if ! command -v lrelease >/dev/null && ! command -v lrelease-qt6 >/dev/null && [[ ! -x /usr/lib/qt6/bin/lrelease ]]; then
    echo "lrelease not found, building without translations"
    sed -i -E '/^\s+(lrelease|embed_translations)\s+\\$/d' OpenRGB.pro
fi

qmake6 OpenRGB.pro CONFIG+=release
make -j"$(nproc)"

echo "Built: $PWD/openrgb"
