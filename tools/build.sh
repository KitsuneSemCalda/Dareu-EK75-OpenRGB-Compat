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
# Last commit verified (CI "Build with OpenRGB") to build cleanly with this
# controller. Cloning the branch tip instead let an unrelated upstream change
# break the default build path with no change on our side. Bump this after
# confirming a newer commit still builds; --update opts out and tracks
# upstream's default branch instead.
OPENRGB_PINNED_COMMIT=4f31ee4ff3962826c424c4ef1ad2b71a66cf5f4e

if [[ ! -d $OPENRGB_DIR ]]; then
    git init -q "$OPENRGB_DIR"
    git -C "$OPENRGB_DIR" remote add origin "$OPENRGB_URL"
    git -C "$OPENRGB_DIR" fetch --depth 1 origin "$OPENRGB_PINNED_COMMIT"
    git -C "$OPENRGB_DIR" checkout -q FETCH_HEAD
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
