#!/usr/bin/env bash
# Install the Omarchy theme-set hook that colours the EK75 with the theme accent.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
HOOK=~/.config/omarchy/hooks/theme-set.d/dareu-ek75

mkdir -p "$(dirname "$HOOK")"
cat >"$HOOK" <<EOF
#!/bin/bash
# Colour the Dareu EK75 with the new theme's accent (see $ROOT/tools/apply-theme.sh)
exec "$ROOT/tools/apply-theme.sh" "\$@"
EOF
chmod +x "$HOOK"
echo "Installed $HOOK"
