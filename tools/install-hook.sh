#!/usr/bin/env bash
# Install the Omarchy theme-set hook that colours the EK75 on every theme change,
# and the colour-transform helper it depends on (COLOR_TRANSFORM in apply-theme.sh).
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
HOOK=~/.config/omarchy/hooks/theme-set.d/dareu-ek75
LIB=~/.local/lib/dareu-ek75

install -Dm755 "$ROOT/tools/apply-theme.sh" "$HOOK"
install -Dm755 "$ROOT/tools/color_transform.py" "$LIB/color_transform.py"
echo "Installed $HOOK"
