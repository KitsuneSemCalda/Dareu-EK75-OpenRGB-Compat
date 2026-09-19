#!/usr/bin/env bash
# Install the Dareu EK75 udev rule and re-apply it to connected hidraw devices.
set -euo pipefail

SELF=$(realpath "$0")
cd "$(dirname "$SELF")/.."
RULE=70-dareu-ek75.rules
DEST=/etc/udev/rules.d

if [[ $EUID -ne 0 ]]; then
    exec sudo "$SELF" "$@"
fi

# Older revisions used a 99- prefix, which runs after 73-seat-late and never gets the ACL.
rm -f "$DEST/99-dareu-ek75.rules"

install -m 644 "udev/$RULE" "$DEST/$RULE"
udevadm control --reload
udevadm trigger --subsystem-match=hidraw --action=change
udevadm trigger --subsystem-match=usb --action=change
echo "Installed $DEST/$RULE"
