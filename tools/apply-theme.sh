#!/usr/bin/env bash
# Set the EK75 key backlight to a colour resolved from an Omarchy theme, keeping whatever
# effect is currently active (Raindrop, Breathing, Static, ...) instead of forcing Static:
# no -m is passed, so OpenRGB's own CLI reuses the device's current mode (see cli.cpp's
# ParseMode(), which falls back to GetActiveMode() when --mode is omitted). An effect with
# no colour slot (e.g. most of the rainbow-cycling ones) is left animating, untouched.
# Usage: apply-theme.sh [theme-name]   (defaults to the current theme)
# The keyboard takes one colour per region, so a full palette can't be shown.
#
# Colour resolution: the theme's own keyboard.rgb file, if it has one, else
# COLOR_KEY (default: accent) from its colors.toml. See docs/THEME_COLOR.md.
#
# If ~/.config/dareu-ek75/color-profile.toml exists, the resolved colour is run
# through tools/color_transform.py first (perceptual calibration + optional
# physical brightness). No profile file: the colour goes to OpenRGB unchanged,
# exactly like before this existed.
set -euo pipefail

OPENRGB=${OPENRGB:-$HOME/.local/lib/dareu-ek75/openrgb}
COLOR_TRANSFORM=${COLOR_TRANSFORM:-$HOME/.local/lib/dareu-ek75/color_transform.py}
COLOR_KEY=${COLOR_KEY:-accent}
PROFILE=${DAREU_COLOR_PROFILE:-${XDG_CONFIG_HOME:-$HOME/.config}/dareu-ek75/color-profile.toml}

name=${1:-$(omarchy theme current)}
slug=$(tr '[:upper:] ' '[:lower:]-' <<<"$name")

for dir in ~/.config/omarchy/themes /usr/share/omarchy/themes; do
    if [[ -f $dir/$slug/keyboard.rgb || -f $dir/$slug/colors.toml ]]; then
        theme_dir=$dir/$slug
        break
    fi
done
[[ -n ${theme_dir:-} ]] || { echo "theme '$name' not found" >&2; exit 1; }

if [[ -f $theme_dir/keyboard.rgb ]]; then
    hex=$(tr -d '[:space:]#' <"$theme_dir/keyboard.rgb")
    [[ $hex =~ ^[0-9a-fA-F]{6}$ ]] || { echo "invalid colour in $theme_dir/keyboard.rgb" >&2; exit 1; }
    color_source="keyboard.rgb"
else
    file=$theme_dir/colors.toml
    hex=$(sed -nE "s/^$COLOR_KEY *= *\"#?([0-9a-fA-F]{6})\".*/\1/p" "$file" | head -1)
    [[ -n $hex ]] || { echo "no '$COLOR_KEY' in $file" >&2; exit 1; }
    color_source=$COLOR_KEY
fi

corrected=$hex
brightness=
if [[ -f $PROFILE ]]; then
    [[ -x $COLOR_TRANSFORM ]] || { echo "$COLOR_TRANSFORM not found, run tools/install-hook.sh" >&2; exit 1; }
    if ! result=$("$COLOR_TRANSFORM" apply --hex "$hex" --profile "$PROFILE" 2>&1); then
        echo "$result" >&2
        exit 1
    fi
    read -r corrected brightness <<<"$result"
fi

args=(--noautoconnect -d "Dareu EK75" -c "$corrected")
[[ -n $brightness ]] && args+=(-b "$brightness")

"$OPENRGB" "${args[@]}"

if [[ ${corrected,,} == "${hex,,}" ]]; then
    echo "EK75 -> #$corrected ($name, $color_source)"
else
    echo "EK75 -> #$corrected (from #$hex, $name, $color_source)"
fi
