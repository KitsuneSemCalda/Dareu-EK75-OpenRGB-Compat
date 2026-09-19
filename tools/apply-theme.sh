#!/usr/bin/env bash
# Set the EK75 key backlight to the accent colour of an Omarchy theme.
# Usage: apply-theme.sh [theme-name]   (defaults to the current theme)
# The keyboard takes one colour per region, so a full palette can't be shown.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OPENRGB=${OPENRGB:-$ROOT/build/OpenRGB/openrgb}
COLOR_KEY=${COLOR_KEY:-accent}

name=${1:-$(omarchy theme current)}
slug=$(tr '[:upper:] ' '[:lower:]-' <<<"$name")

for dir in ~/.config/omarchy/themes /usr/share/omarchy/themes; do
    [[ -f $dir/$slug/colors.toml ]] && file=$dir/$slug/colors.toml && break
done
[[ -n ${file:-} ]] || { echo "theme '$name' not found" >&2; exit 1; }

hex=$(sed -nE "s/^$COLOR_KEY *= *\"#?([0-9a-fA-F]{6})\".*/\1/p" "$file" | head -1)
[[ -n $hex ]] || { echo "no '$COLOR_KEY' in $file" >&2; exit 1; }

"$OPENRGB" --noautoconnect -d "Dareu EK75" -m Static -c "$hex"
echo "EK75 -> #$hex ($name, $COLOR_KEY)"
