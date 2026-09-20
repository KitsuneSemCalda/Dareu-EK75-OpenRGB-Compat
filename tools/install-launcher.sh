#!/usr/bin/env bash
# Make the menu entry and the login autostart run the OpenRGB build that has the EK75 driver,
# instead of /usr/bin/openrgb, which doesn't know this keyboard.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SOURCE=${OPENRGB:-$ROOT/build/OpenRGB/openrgb}
BIN=$HOME/.local/lib/dareu-ek75/openrgb
SYSTEM_ENTRY=${SYSTEM_ENTRY:-/usr/share/applications/org.openrgb.OpenRGB.desktop}
MENU_ENTRY=~/.local/share/applications/org.openrgb.OpenRGB.desktop
AUTOSTART=~/.config/autostart/OpenRGB.desktop

[[ -x $SOURCE ]] || { echo "$SOURCE not built yet, run tools/build.sh" >&2; exit 1; }
[[ -f $SYSTEM_ENTRY ]] || { echo "$SYSTEM_ENTRY not found" >&2; exit 1; }
install -Dm755 "$SOURCE" "$BIN"

# Same file name as the system entry, so it takes over the menu launcher.
mkdir -p "$(dirname "$MENU_ENTRY")"
sed -E "s|^Exec=.*|Exec=$BIN|" "$SYSTEM_ENTRY" >"$MENU_ENTRY"
echo "Installed $MENU_ENTRY"

if [[ -f $AUTOSTART ]]; then
    sed -i -E "s|^Exec=.*|Exec=$BIN|" "$AUTOSTART"
    echo "Updated $AUTOSTART"
fi
