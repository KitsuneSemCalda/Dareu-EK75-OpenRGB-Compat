#!/usr/bin/env bash
# Install the Omarchy theme-set hook that colours the EK75 with the theme accent.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
HOOK=~/.config/omarchy/hooks/theme-set.d/dareu-ek75

install -Dm755 "$ROOT/tools/apply-theme.sh" "$HOOK"
echo "Installed $HOOK"
