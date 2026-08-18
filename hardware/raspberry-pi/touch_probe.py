#!/usr/bin/env python3
"""Capture five-point raw evdev data from an ADS7846/XPT2046 touchscreen."""

from __future__ import annotations

import argparse
import json
import os
import select
import struct
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


EVENT = struct.Struct("llHHi")
EV_KEY = 0x01
EV_ABS = 0x03
BTN_TOUCH = 0x14A
ABS_X = 0x00
ABS_Y = 0x01
ABS_PRESSURE = 0x18
POINTS = ("center", "top-left", "top-right", "bottom-left", "bottom-right")


def find_ads7846() -> Path:
    blocks = Path("/proc/bus/input/devices").read_text().split("\n\n")
    for block in blocks:
        if 'N: Name="ADS7846 Touchscreen"' not in block:
            continue
        for line in block.splitlines():
            if line.startswith("H: Handlers="):
                event = next((x for x in line.split() if x.startswith("event")), None)
                if event:
                    return Path("/dev/input") / event
    raise RuntimeError("ADS7846 Touchscreen was not found in /proc/bus/input/devices")


def median(values: list[int]) -> int | None:
    if not values:
        return None
    ordered = sorted(values)
    return ordered[len(ordered) // 2]


def capture_press(fd: int, timeout: float) -> dict:
    deadline = time.monotonic() + timeout
    touching = False
    samples: list[dict] = []
    current: dict[str, int] = {}

    while time.monotonic() < deadline:
        ready, _, _ = select.select([fd], [], [], min(0.25, deadline - time.monotonic()))
        if not ready:
            continue
        raw = os.read(fd, EVENT.size)
        if len(raw) != EVENT.size:
            continue
        sec, usec, event_type, code, value = EVENT.unpack(raw)
        if event_type == EV_KEY and code == BTN_TOUCH:
            if value:
                touching = True
            elif touching:
                break
        elif event_type == EV_ABS:
            if code == ABS_X:
                current["x"] = value
            elif code == ABS_Y:
                current["y"] = value
            elif code == ABS_PRESSURE:
                current["pressure"] = value
            if touching and "x" in current and "y" in current:
                samples.append({
                    "time": sec + usec / 1_000_000,
                    "x": current["x"],
                    "y": current["y"],
                    "pressure": current.get("pressure"),
                })

    if not samples:
        raise TimeoutError("no complete touch samples received")
    return {
        "sample_count": len(samples),
        "x": median([s["x"] for s in samples]),
        "y": median([s["y"] for s in samples]),
        "pressure": median([s["pressure"] for s in samples if s["pressure"] is not None]),
        "raw_samples": samples,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", type=Path, help="evdev path (auto-detected by default)")
    parser.add_argument("--output", type=Path, help="JSON output path")
    parser.add_argument("--timeout", type=float, default=15.0, help="seconds allowed per point")
    args = parser.parse_args()

    device = args.device or find_ads7846()
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    output = args.output or Path(f"touch-probe-{stamp}.json")
    result = {
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "device": str(device),
        "event_struct_size": EVENT.size,
        "points": {},
    }

    print(f"Device: {device}")
    print("For each prompt, press the named point for about one second, then release.\n")
    try:
        fd = os.open(device, os.O_RDONLY | os.O_CLOEXEC)
    except PermissionError:
        print(f"Permission denied: {device}", file=sys.stderr)
        print("Check the device permissions/group; do not use sudo unless approved.", file=sys.stderr)
        return 2

    try:
        for point in POINTS:
            input(f"Press Enter when ready for {point}, then touch and release: ")
            try:
                reading = capture_press(fd, args.timeout)
            except TimeoutError as exc:
                print(f"Failed at {point}: {exc}", file=sys.stderr)
                return 3
            result["points"][point] = reading
            print(
                f"  {point}: x={reading['x']} y={reading['y']} "
                f"pressure={reading['pressure']} samples={reading['sample_count']}"
            )
    finally:
        os.close(fd)

    output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"\nSaved: {output.resolve()}")
    print("Send this JSON file or paste the five summary lines for calibration analysis.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
