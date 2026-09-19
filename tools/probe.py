#!/usr/bin/env python3
"""Probe the Dareu EK75 vendor HID interface (stdlib only).

Usage: probe.py [--target 0x10]
Protocol reference: https://github.com/mateusands/open-ek75 (PROTOCOL.md)
"""
import argparse, fcntl, glob, os, struct, sys, time

VID = 0x260D
GET, SET = 0x80, 0x00
CLASS_DEVICE, CLASS_LIGHTING = 0, 3
LED_EFFECT, LED_BRIGHTNESS = 2, 3


def _ioc(nr, size):
    return (3 << 30) | (size << 16) | (ord("H") << 8) | nr


def find_hidraw():
    for node in sorted(glob.glob("/sys/class/hidraw/hidraw*")):
        with open(f"{node}/device/uevent") as f:
            if f"{VID:04X}" not in f.read().upper():
                continue
        desc = open(f"{node}/device/report_descriptor", "rb").read()
        if bytes.fromhex("0600ff") in desc and bytes.fromhex("954" + "0b1") in desc:
            return "/dev/" + os.path.basename(node)
    sys.exit("vendor hidraw interface not found")


def xfer(fd, target, size, cls, cmd, profile=1, payload=b""):
    buf = bytearray(65)  # [0] = report id (0)
    buf[1], buf[2], buf[3], buf[4] = target, size, cls, cmd
    buf[5] = profile
    buf[7:7 + len(payload)] = payload
    fcntl.ioctl(fd, _ioc(0x06, 65), buf)
    for _ in range(20):
        time.sleep(0.01)
        rep = bytearray(65)
        fcntl.ioctl(fd, _ioc(0x07, 65), rep)
        if rep[1] & 0x0F == 2:
            return bytes(rep[1:])
    raise TimeoutError("no reply")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", type=lambda x: int(x, 0), default=0)
    a = ap.parse_args()
    path = find_hidraw()
    print("using", path)
    fd = os.open(path, os.O_RDWR)
    r = xfer(fd, 0, 7, CLASS_DEVICE, 32 | GET, profile=0)
    print("wireless status:", r[:16].hex(" "))
    r = xfer(fd, a.target, 1, CLASS_LIGHTING, LED_EFFECT | GET, payload=b"\x01")
    print("effect region1:", r[:20].hex(" "))
    r = xfer(fd, a.target, 2, CLASS_LIGHTING, LED_BRIGHTNESS | GET, payload=b"\x01")
    print("brightness region1:", r[:12].hex(" "))


if __name__ == "__main__":
    main()
